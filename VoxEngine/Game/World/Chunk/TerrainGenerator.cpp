#include "TerrainGenerator.h"

#include "Core/Profiler.h"

#include <iostream>
#include <cmath>

//============================================================================
float continentalSpline(float x)
{
	return x;
}


//float continentalSpline(float x)
//{
//	constexpr float power = 5.0f;
//
//	x = (x + 1.0f) * 0.5f;
//	float t = power * x + 0.5f * (1.0f - power);
//	t = fminf(1.0f, fmaxf(0.0, t));
//	return 3.0f * t * t - 2.0f * t * t * t;
//
//	float y1 = 10.0f * (1.0f - 20.0f * x);
//	float y3 = 10.0f * (x - 0.25f);
//	float y5 = 10.0f * (x - 0.5f);
//	float y6 = 1.0f + 0.1f * (x - 1.0f);
//	
//	float r1 = fminf(y3, 0.5f);
//	float r2 = fmaxf(r1, y5);
//	float r3 = fminf(r2, y6);
//	float r4 = fmaxf(y1, 0.0f);
//	float r5 = fmaxf(r4, r3);
//	float r6 = fminf(r5, 1.0f);
//
//	return r6;
//}

//float continentalSpline(float x)
//{
//	constexpr float a = 0.5f;
//	constexpr float b = 0.07f;
//	constexpr float s = 3.0f;
//
//	auto f = [](float t)
//		{
//			return sqrtf(t);
//		};
//	auto g = [](float t)
//		{
//			return floorf(t * s) / s;
//		};
//
//	float t = (x + 1.0f) * 0.5f;
//	float value;
//	if (t >= a)
//	{
//		value = f((t - a) / (1.0f - a)) * b + 1.0f - b;
//	}
//	else
//	{
//		value = g(t / a) * (1.0f - b);
//	}
//	return value;
//}

//float continentalSpline(float x)
//{
//	constexpr float N = 3.0f;
//
//	x = (x + 1.0f) * 0.5f;
//	float scaled = x * N;
//	float f = floorf(scaled);
//	float t = scaled - f;
//	float s = t * t * (3.0f - 2.0f * t);
//	return (f + s) / N;
//}

//============================================================================
//ChunkColumnData

ChunkColumnData::ChunkColumnData()
{
}

ChunkColumnData::~ChunkColumnData()
{
}

void ChunkColumnData::init(int x, int z)
{
	X = x; Z = z;
}

void ChunkColumnData::destroy()
{
	referenceCount = 0;
	initialized = false;
}

const int* ChunkColumnData::heightMapRead() const
{
	PROFILE_SCOPE("Read height map", ProfileCategory::ChunkColumnData);

	std::unique_lock<std::mutex> lock(readDataMutex);
	readDataCV.wait(lock, [this]() { return initialized; });
	return heightMap;
}

int* ChunkColumnData::heightMapWrite()
{
	return heightMap;
}

void ChunkColumnData::setToInitialized()
{
	{
		std::lock_guard<std::mutex> lock(readDataMutex);
		initialized = true;
	}
	readDataCV.notify_all();
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
thread_local std::vector<float> TerrainGenerator::internalLayeredNoiseArray;
thread_local std::vector<float> TerrainGenerator::caveNoiseArray;

TerrainGenerator& TerrainGenerator::getInstance()
{
	static TerrainGenerator instance;
	return instance;
}

void TerrainGenerator::initThread()
{
	simplexNoise = FastNoise::New<FastNoise::Simplex>();
	internalLayeredNoiseArray.resize(CHUNK_VOLUME);
	caveNoiseArray.resize(CHUNK_VOLUME);
	std::this_thread::sleep_for(std::chrono::milliseconds(10)); // his sleep makes sure values will initialize
}

const ChunkColumnData* TerrainGenerator::loadChunkColumnData(int chunkX, int chunkZ)
{
	Profiler::beginProfile("Load ChunkColumnData", ProfileCategory::ChunkColumnData);

	glm::ivec2 pos(chunkX, chunkZ);

	// Check if column already exists
	{
		ChunkColumnData* foundColumn = nullptr;
		bool isColumnFound = false;
		{
			std::lock_guard<std::mutex> lock(dataMutex);
			auto it = chunkColumnData.find(pos);
			isColumnFound = it != chunkColumnData.end();
			if (isColumnFound)
			{
				foundColumn = it->second.get();
			}
		}
		if (isColumnFound)
		{
			foundColumn->referenceCount.fetch_add(1, std::memory_order_acq_rel);
			Profiler::endProfile();
			return foundColumn;
		}
	}

	// Create column
	std::unique_ptr<ChunkColumnData> column = chunkColumnDataPool.acquire();
	column->referenceCount = 1;
	ChunkColumnData* columnPtr = column.get();

	{
		// Check again in case another thread created it while we were acquiring from pool
		std::lock_guard<std::mutex> lock(dataMutex);

		ChunkColumnData* foundColumn = nullptr;
		bool isColumnFound = false;
		{
			auto it = chunkColumnData.find(pos);
			isColumnFound = it != chunkColumnData.end();
			if (isColumnFound)
			{
				foundColumn = it->second.get();
			}
		}
		// Mutex could be unlocked here, but I think it will cause problems, creating a gap when other thread checks the stuff.
		// This thread must book the place for itself for creating the column.
		if (isColumnFound)
		{
			// Another thread beat us to it, return the column to the pool
			chunkColumnDataPool.release(std::move(column));
			foundColumn->referenceCount.fetch_add(1, std::memory_order_acq_rel);
			Profiler::endProfile();
			return foundColumn;
		}

		// Move column into the map
		chunkColumnData.emplace(pos, std::move(column));
	}
	Profiler::endProfile();
	{
		initChunkColumnData(columnPtr, chunkX, chunkZ);
		columnPtr->setToInitialized();
	}
	return columnPtr;
}

const ChunkColumnData* TerrainGenerator::getChunkColumnData(int chunkX, int chunkZ)
{
	PROFILE_SCOPE("Get ChunkColumnData", ProfileCategory::ChunkColumnData);

	glm::ivec2 pos(chunkX, chunkZ);

	ChunkColumnData* foundColumn = nullptr;
	{
		std::lock_guard<std::mutex> lock(dataMutex);
		auto it = chunkColumnData.find(pos);
		bool isColumnFound = it != chunkColumnData.end();
		if (isColumnFound)
		{
			foundColumn = it->second.get();
		}
	}
	return foundColumn;
}

void TerrainGenerator::unloadChunkColumnData(int chunkX, int chunkZ)
{
	// TODO: (When moving down) Chunks on edge of render area may unload, they are last, destroying the column.
	// Maybe get rid of unused columns in loop after creating chunks. Maybe use time or distance to determine lifetime of unused column.
	PROFILE_SCOPE("Unload ChunkColumnData", ProfileCategory::ChunkColumnData);

	// TODO: Try to use mutex only when reading map and erasing from the map, not when loading referenceCount. Though, maybe it will produce bugs.
	std::unique_ptr<ChunkColumnData> columnToRelease;
	{
		std::lock_guard<std::mutex> lock(dataMutex);

		glm::ivec2 pos(chunkX, chunkZ);
		auto it = chunkColumnData.find(pos);
		if (it == chunkColumnData.end())
		{
			return;
		}

		// Decrement reference count
		auto oldReferenceCount = it->second->referenceCount.fetch_sub(1, std::memory_order_acq_rel);
		// 'oldReferenceCount' has value of referenceCount before decrement operation.

		// If no more references, unload the column
		if (oldReferenceCount - 1 <= 0)
		{
			columnToRelease = std::move(it->second);
			chunkColumnData.erase(it);
		}
	}
	if (columnToRelease)
	{
		chunkColumnDataPool.release(std::move(columnToRelease));
	}
}

void TerrainGenerator::computeCaveMask(bool* outArray, int chunkX, int chunkY, int chunkZ)
{
	// TODO: Try computing low-resolution noise array and interpolate it
	PROFILE_SCOPE("Compute cave mask", ProfileCategory::TerrainGeneration);

	{
		NoiseParams params;
		params.frequency = 0.01f;
		params.layerCount = 3;
		computeLayeredNoise_3D(caveNoiseArray.data(), chunkX, chunkY, chunkZ, params);
	}

	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				const int readIndex = z + y * CHUNK_SIZE + x * CHUNK_AREA;
				const int writeIndex = x + y * CHUNK_SIZE + z * CHUNK_AREA;

				float value = caveNoiseArray[readIndex];
				value = fabsf(value);
				value = fminf(value * 5.0f, 1.0f);
				value = 1.0f - value;

				outArray[writeIndex] = value > 0.5f;
			}
		}
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

	computeInitialHeightMap(column->heightMapWrite(), chunkX, chunkZ);
}

float TerrainGenerator::calculateHeight(float continentalNoise, float erosionNoise, float weirdnessNoise)
{
	// Params
	constexpr float continentalAmplitude = 100.0f;
	constexpr float erosionAmplitude = 10.0f;
	constexpr float erosionThreshold = 0.8f;
	constexpr float weirdnessAmplitude = 20.0f;
	constexpr float beachThreshold = 10.0f / continentalAmplitude;

	// Continental
	float continentalNoiseSpline = continentalSpline(continentalNoise);
	float continentalHeight = continentalNoiseSpline * continentalAmplitude;
	float height = continentalHeight;

	float beachOcean;
	if (continentalNoiseSpline < 0.0f)
	{
		beachOcean = 1.0f;
	}
	else if (continentalNoiseSpline > beachThreshold)
	{
		beachOcean = 0.0f;
	}
	else
	{
		beachOcean = 1.0 - continentalNoiseSpline / beachThreshold;
	}

	// Erosion
	erosionNoise = (erosionNoise + 1.0f) * 0.5f;
	if (erosionNoise > erosionThreshold)
	{
		float erosionNormalized = (erosionNoise - erosionThreshold) / (1.0f - erosionThreshold);
		height -= erosionNormalized * erosionAmplitude * (1.0f - beachOcean);
	}

	// Weirdness
	weirdnessNoise = (weirdnessNoise + 1.0f) * 0.5f;
	float weirdnessHeight = (1.0f - fabsf(3.0f * weirdnessNoise - 2.0f)) * weirdnessAmplitude * (1.0f - beachOcean);
	height += weirdnessHeight;

	return height;
}

void TerrainGenerator::computeInitialHeightMap(int* heightMap, int chunkX, int chunkZ)
{
	PROFILE_SCOPE("Compute height map", ProfileCategory::TerrainGeneration);

	// Computing continental noise array
	float continentalNoiseArray[CHUNK_AREA];
	{
		NoiseParams params;
		params.frequency = 0.0001f;
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
		params.frequency = 0.005f;
		params.layerCount = 3;
		params.amplitudeFactor = 0.25f;
		params.frequencyFactor = 4.0f;
		computeLayeredNoise_2D(weirdnessNoiseArray, chunkX, chunkZ, params);
	}

	// Fill height map. Calculate new values. Flip X and Z because FastNoise does wrong orientation.
	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int z = 0; z < CHUNK_SIZE; z++)
		{
			const int readIndex = z + x * CHUNK_SIZE;

			float continentalNoise = continentalNoiseArray[readIndex];
			float erosionNoise = erosionNoiseArray[readIndex];
			float weirdnessNoise = weirdnessNoiseArray[readIndex];

			heightMap[x + z * CHUNK_SIZE] = (int)calculateHeight(continentalNoise, erosionNoise, weirdnessNoise);
		}
	}
}

void TerrainGenerator::computeNoise_2D(float* outArray, int chunkX, int chunkZ, float frequency)
{
	simplexNoise->GenUniformGrid2D(outArray, chunkX * CHUNK_SIZE, chunkZ * CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, frequency, seed);
}

void TerrainGenerator::computeLayeredNoise_2D(float* outArray, int chunkX, int chunkZ, const NoiseParams& params)
{
	for (int i = 0; i < CHUNK_AREA; i++)
	{
		outArray[i] = 0.0f;
	}

	float layerAmplitude = 1.0f;
	float layerFrequency = params.frequency;
	float amplitudeSum = 0.0f;

	for (int i = 0; i < params.layerCount; i++)
	{
		computeNoise_2D(internalLayeredNoiseArray.data(), chunkX, chunkZ, layerFrequency);
		for (int index = 0; index < CHUNK_AREA; index++)
		{
			outArray[index] += internalLayeredNoiseArray[index] * layerAmplitude;
		}
		amplitudeSum += layerAmplitude;
		layerAmplitude *= params.amplitudeFactor;
		layerFrequency *= params.frequencyFactor;
	}

	if (amplitudeSum != 1.0f && amplitudeSum != 0.0f)
	{
		float factor = 1.0f / amplitudeSum;
		for (int i = 0; i < CHUNK_AREA; i++)
		{
			outArray[i] *= factor;
		}
	}
}

void TerrainGenerator::computeNoise_3D(float* outArray, int chunkX, int chunkY, int chunkZ, float frequency)
{
	simplexNoise->GenUniformGrid3D(outArray, chunkX * CHUNK_SIZE, chunkY * CHUNK_SIZE, chunkZ * CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, frequency, seed);
}

void TerrainGenerator::computeLayeredNoise_3D(float* outArray, int chunkX, int chunkY, int chunkZ, const NoiseParams& params)
{
	for (int i = 0; i < CHUNK_VOLUME; i++)
	{
		outArray[i] = 0.0f;
	}

	float layerAmplitude = 1.0f;
	float layerFrequency = params.frequency;

	float amplitudeSum = 0.0f;

	for (int i = 0; i < params.layerCount; i++)
	{
		computeNoise_3D(internalLayeredNoiseArray.data(), chunkX, chunkY, chunkZ, layerFrequency);
		for (int index = 0; index < CHUNK_VOLUME; index++)
		{
			outArray[index] += internalLayeredNoiseArray[index] * layerAmplitude;
		}
		amplitudeSum += layerAmplitude;
		layerAmplitude *= params.amplitudeFactor;
		layerFrequency *= params.frequencyFactor;
	}

	if (amplitudeSum != 1.0f && amplitudeSum != 0.0f)
	{
		float factor = 1.0f / amplitudeSum;
		for (int i = 0; i < CHUNK_VOLUME; i++)
		{
			outArray[i] *= factor;
		}
	}
}

//============================================================================

size_t Int2Hasher::operator()(const glm::ivec2& other) const
{
	constexpr size_t addConst = 0x9e3779b97f4a7c15;
	size_t h = (size_t)other.x + addConst;
	h ^= (size_t)other.y + addConst + (h << 6) + (h >> 2);
	return h;
}
