#include "TerrainGenerator.h"

#include "Profiler.h"

#include <iostream>
#include <cmath>

//============================================================================
//ChunkColumnData

ChunkColumnData::ChunkColumnData() :
	referenceCount(0)
{
}

ChunkColumnData::~ChunkColumnData()
{
}

void ChunkColumnData::init(int x, int z)
{
	X = x; Z = z;
	referenceCount = 0;
}

void ChunkColumnData::destroy()
{
	referenceCount = 0;
}

//============================================================================
//ChunkColumnDataPool

std::unique_ptr<ChunkColumnData> TerrainGenerator::ChunkColumnDataPool::acquire()
{
	std::lock_guard<std::mutex> lock(poolMutex);
	if (!pool.empty())
	{
		std::unique_ptr<ChunkColumnData> chunkColumnData = std::move(pool.back());
		pool.pop_back();
		return chunkColumnData;
	}
	return std::make_unique<ChunkColumnData>();
}

void TerrainGenerator::ChunkColumnDataPool::release(std::unique_ptr<ChunkColumnData> chunkColumnData)
{
	chunkColumnData->destroy();

	std::lock_guard<std::mutex> lock(poolMutex);
	pool.push_back(std::move(chunkColumnData));
}

//============================================================================
//TerrainGenerator

int TerrainGenerator::seed = 0;
thread_local FastNoise::SmartNode<FastNoise::Simplex> TerrainGenerator::simplexNoise;

TerrainGenerator& TerrainGenerator::getInstance()
{
	static TerrainGenerator instance;
	return instance;
}

const ChunkColumnData* TerrainGenerator::loadChunkColumnData(int chunkX, int chunkZ)
{
	// TODO: Don't check two times

	Int2 pos(chunkX, chunkZ);

	// Check if column already exists
	{
		std::lock_guard<std::mutex> lock(dataMutex);
		auto it = chunkColumnData.find(pos);
		if (it != chunkColumnData.end())
		{
			it->second->referenceCount++;
			return it->second.get();
		}
	}

	// Create column
	std::unique_ptr<ChunkColumnData> column = chunkColumnDataPool.acquire();

	// Check again in case another thread created it while we were acquiring from pool
	// TODO: Mutex prevents generating height maps in parallel. Without it, bugs appear. FIX IT!
	{
		std::lock_guard<std::mutex> lock(dataMutex);
		auto it = chunkColumnData.find(pos);
		if (it != chunkColumnData.end())
		{
			// Another thread beat us to it, return the column to the pool
			dataMutex.unlock();
			chunkColumnDataPool.release(std::move(column));
			dataMutex.lock();

			it->second->referenceCount++;
			return it->second.get();
		}

		// Move column into the map
		auto inserted = chunkColumnData.insert(std::make_pair(pos, std::move(column)));

		// Init column
		ChunkColumnData* columnPtr = inserted.first->second.get();
		initChunkColumnData(columnPtr, chunkX, chunkZ);
		columnPtr->referenceCount = 1;

		return columnPtr;
	}
}

void TerrainGenerator::releaseChunkColumnData(int chunkX, int chunkZ)
{
	//PROFILE_SCOPE("Release chunk column data"); TODO: Make Profiler thread safe

	std::lock_guard<std::mutex> lock(dataMutex);

	Int2 pos(chunkX, chunkZ);
	auto it = chunkColumnData.find(pos);
	if (it == chunkColumnData.end())
	{
		return;
	}

	// Decrement reference count
	it->second->referenceCount--;

	// If no more references, unload the column
	if (it->second->referenceCount <= 0)
	{
		std::unique_ptr<ChunkColumnData> columnToRelease = std::move(it->second);
		chunkColumnData.erase(it);

		// Release to pool without holding the data mutex
		dataMutex.unlock();
		chunkColumnDataPool.release(std::move(columnToRelease));
		dataMutex.lock();
	}
}

size_t TerrainGenerator::getChunkColumnDataCount() const
{
	std::lock_guard<std::mutex> lock(dataMutex);
	return chunkColumnData.size();
}

void TerrainGenerator::initChunkColumnData(ChunkColumnData* column, int chunkX, int chunkZ)
{
	column->init(chunkX, chunkZ);

	if (simplexNoise.get() == nullptr)
	{
		simplexNoise = FastNoise::New<FastNoise::Simplex>();
	}

	computeInitialHeightMap(column->heightMap, chunkX, chunkZ);
}

void TerrainGenerator::computeInitialHeightMap(int* heightMap, int chunkX, int chunkZ)
{
	PROFILE_SCOPE("Compute height map");

	// Computing continental noise array
	float continentalNoiseArray[CHUNK_AREA];
	{
		NoiseParams params;
		params.frequency = 0.00001f;
		params.layerCount = 3;
		computeLayeredNoise_2D(continentalNoiseArray, chunkX, chunkZ, params);
	}

	float erosionNoiseArray[CHUNK_AREA];
	{
		NoiseParams params;
		params.frequency = 0.01f;
		params.layerCount = 1;
		computeLayeredNoise_2D(erosionNoiseArray, chunkX, chunkZ, params);
	}

	float weirdnessNoiseArray[CHUNK_AREA];
	{
		NoiseParams params;
		params.frequency = 0.0025f;
		params.layerCount = 3;
		params.amplitudeFactor = 0.25f;
		params.frequencyFactor = 4.0f;
		computeLayeredNoise_2D(weirdnessNoiseArray, chunkX, chunkZ, params);
	}

	const float continentalAmplitude = 200.0f;

	const float erosionAmplitude = 10.0f;
	const float erosionThreshold = 0.8f;

	const float weirdnessAmplitude = 20.0f;

	// Fill height map. Calaculate new values. Flip X and Z because FastNoise does wrong orientation.
	// TODO: Add spline for continental noise
	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int z = 0; z < CHUNK_SIZE; z++)
		{
			const int readIndex = z + x * CHUNK_SIZE;

			// Load noise values
			float continentalNoise = continentalNoiseArray[readIndex];
			float erosionNoise = erosionNoiseArray[readIndex];
			float weirdnessNoise = weirdnessNoiseArray[readIndex];

			// Continental
			float continentalHeight = (continentalNoise * 2.0f - 1.0f) * continentalAmplitude;
			float height = continentalHeight;

			// Erosion
			if (erosionNoise > erosionThreshold)
			{
				height -= (erosionNoise - erosionThreshold) / (1.0f - erosionThreshold) * erosionAmplitude;
			}

			// Weirdness
			float weirdnessHeight = (1.0f - fabsf(3.0f * weirdnessNoise - 2.0f)) * weirdnessAmplitude;
			height += weirdnessHeight;

			heightMap[x + z * CHUNK_SIZE] = (int)height;
		}
	}
}

void TerrainGenerator::computeNoise_2D(float* noiseArray, int chunkX, int chunkZ, float frequency)
{
	simplexNoise->GenUniformGrid2D(noiseArray, chunkX * CHUNK_SIZE, chunkZ * CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, frequency, seed);
}

void TerrainGenerator::computeLayeredNoise_2D(float* noiseArray, int chunkX, int chunkZ, const NoiseParams& params)
{
	for (int i = 0; i < CHUNK_AREA; i++)
	{
		noiseArray[i] = 0.0f;
	}

	float layerAmplitude = params.amplitude;
	float layerFrequency = params.frequency;

	float tempNoiseArray[CHUNK_AREA];

	for (int i = 0; i < params.layerCount; i++)
	{
		computeNoise_2D(tempNoiseArray, chunkX, chunkZ, layerFrequency);
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				int index = z + x * CHUNK_SIZE;
				float value = tempNoiseArray[index];
				value = (value + 1.0f) * 0.5f * layerAmplitude;
				noiseArray[index] += value;
			}
		}
		layerAmplitude *= params.amplitudeFactor;
		layerFrequency *= params.frequencyFactor;
	}

	const float invMaxSum = (params.amplitudeFactor == 1.0f || params.layerCount == 1) ?
		1.0f / params.layerCount :
		(1.0f - params.amplitudeFactor) / (1.0f - powf(params.amplitudeFactor, params.layerCount));

	if (invMaxSum != 1.0f)
	{
		for (int i = 0; i < CHUNK_AREA; i++)
		{
			noiseArray[i] *= invMaxSum;
		}
	}
}

//============================================================================