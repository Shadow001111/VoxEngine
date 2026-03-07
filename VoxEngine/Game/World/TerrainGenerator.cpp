#include "TerrainGenerator.h"

#include "Core/Profiler.h"

#include <intrin.h>

//============================================================================
float continentalSpline(float x)
{
	return x;
}


#define MIN_FUNCTION(a, b) a < b ? a : b
#define MAX_FUNCTION(a, b) a > b ? a : b
#define ABS_FUNCTION(a) std::abs(a) //a > 0 ? a : -a;


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
	std::unique_lock<std::mutex> lock(readDataMutex);
	readDataCV.wait(lock, [this]() { return initialized; });
	return heightMap.data();
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

ChunkColumnData* TerrainGenerator::ChunkColumnDataPool::acquire()
{
	std::lock_guard<std::mutex> lock(poolMutex);
	return FixedArenaObjectPool::acquire();
}

void TerrainGenerator::ChunkColumnDataPool::release(ChunkColumnData* chunkColumnData)
{
	chunkColumnData->destroy();

	std::lock_guard<std::mutex> lock(poolMutex);
	pool.push_back(chunkColumnData);
}

//============================================================================
//TerrainGenerator

int TerrainGenerator::seed = 0;
thread_local TerrainGenerator::ThreadLocalData TerrainGenerator::threadLocalData;

TerrainGenerator& TerrainGenerator::getInstance()
{
	static TerrainGenerator instance;
	return instance;
}

const ChunkColumnData* TerrainGenerator::loadChunkColumnData(int chunkX, int chunkZ)
{
	ChunkColumnData* column;
	{
		PROFILE_SCOPE("Load ChunkColumnData", ProfileCategory::ChunkColumnData);

		glm::ivec2 pos(chunkX, chunkZ);

		// Check if column already exists
		{
			std::lock_guard<std::mutex> lock(dataMutex);
			auto it = chunkColumnData.find(pos);
			if (it != chunkColumnData.end())
			{
				it->second->referenceCount.fetch_add(1, std::memory_order_acq_rel);
				return it->second;
			}
		}

		// Create column
		column = chunkColumnDataPool.acquire();
		column->referenceCount.store(1, std::memory_order_relaxed);

		{
			// Check again in case another thread created it while we were acquiring from pool
			std::lock_guard<std::mutex> lock(dataMutex);

			ChunkColumnData* foundColumn = nullptr;
			bool isColumnFound = false;
			{
				const auto it = chunkColumnData.find(pos);
				isColumnFound = it != chunkColumnData.end();
				if (isColumnFound)
				{
					foundColumn = it->second;
				}
			}
			// Mutex could be unlocked here, but I think it will cause problems, creating a gap when other thread checks the stuff.
			// This thread must book the place for itself for creating the column.
			if (isColumnFound)
			{
				// Another thread beat us to it, return the column to the pool
				chunkColumnDataPool.release(column);
				foundColumn->referenceCount.fetch_add(1, std::memory_order_acq_rel);
				return foundColumn;
			}

			// Move column into the map
			chunkColumnData.emplace(pos, column);
		}
	}
	initChunkColumnData(column, chunkX, chunkZ);
	return column;
}

const ChunkColumnData* TerrainGenerator::getChunkColumnData(int chunkX, int chunkZ)
{
	PROFILE_SCOPE("Get ChunkColumnData", ProfileCategory::ChunkColumnData);

	glm::ivec2 pos(chunkX, chunkZ);

	std::lock_guard<std::mutex> lock(dataMutex);
	const auto& it = chunkColumnData.find(pos);
	if (it != chunkColumnData.end())
	{
		return it->second;
	}
	return nullptr;
}

void TerrainGenerator::unloadChunkColumnData(int chunkX, int chunkZ)
{
	PROFILE_SCOPE("Unload ChunkColumnData", ProfileCategory::ChunkColumnData);

	ChunkColumnData* columnToRelease = nullptr;
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

		// If no more references, unload the column
		if (oldReferenceCount <= 1)
		{
			columnToRelease = it->second;
			chunkColumnData.erase(it);
		}
	}
	if (columnToRelease)
	{
		chunkColumnDataPool.release(columnToRelease);
	}
}

void TerrainGenerator::computeCaveMask(bool* outArray, int chunkX, int chunkY, int chunkZ) const
{
	// TODO: Try computing low-resolution noise array and interpolate it

	float* caveNoiseArray = threadLocalData.resources->caveNoiseArray.data();

	{
		PROFILE_SCOPE("Cave mask: compute noises", ProfileCategory::TerrainGeneration);

		NoiseParams params;
		params.frequency = 0.01f;
		params.layerCount = 3;
		computeLayeredNoise_3D(caveNoiseArray, chunkX, chunkY, chunkZ, params);
	}

	{
		PROFILE_SCOPE("Cave mask: combine noises", ProfileCategory::TerrainGeneration);

		//for (size_t i = 0; i < CHUNK_VOLUME; i++)
		//{
		//	float value = caveNoiseArray[i];
		//	value = ABS_FUNCTION(value);
		//
		//	outArray[i] = value < 0.1f;
		//}

		const __m128 abs_mask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)); // sign-strip mask
		const __m128 threshold = _mm_set1_ps(0.1f);
		const __m128i true_mask = _mm_set1_epi8(1);

		for (int i = 0; i < CHUNK_VOLUME; i += 4)
		{
			__m128 values = _mm_loadu_ps(&caveNoiseArray[i]);         // load 4 floats
			__m128 absoluteValues = _mm_and_ps(values, abs_mask);           // fabsf x4
			__m128i comparisons = _mm_castps_si128(_mm_cmplt_ps(absoluteValues, threshold));     // abs(x) < 0.1f | 4 bytes (0x01 or 0x00)

			// Pack bools
			comparisons = _mm_packs_epi32(comparisons, comparisons);
			comparisons = _mm_packs_epi16(comparisons, comparisons);
			comparisons = _mm_and_si128(comparisons, true_mask);

			// Store 4 bools at once
			*((int32_t*)(outArray + i)) = _mm_cvtsi128_si32(comparisons);
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
	auto heightMap = column->heightMapWrite();
	computeInitialHeightMap(heightMap, chunkX, chunkZ);
	column->maxHeight = computeMaxHeight(heightMap);
	column->setToInitialized();
}

float TerrainGenerator::calculateHeight(float continentalNoise, float erosionNoise, float weirdnessNoise)
{
	// Params
	constexpr float continentalAmplitude = 100.0f;
	constexpr float erosionAmplitude = 10.0f;
	constexpr float erosionThreshold = 0.8f;
	constexpr float weirdnessAmplitude = 20.0f;
	constexpr float beachThreshold = 10.0f / continentalAmplitude;

	// Precompute for small performance
	constexpr float invBeachThreshold = 1.0f / beachThreshold;
	constexpr float invOneMinusErosionThreshold = 1.0f / (1.0f - erosionThreshold);

	// Continental
	float continentalNoiseSpline = continentalSpline(continentalNoise);
	float continentalHeight = continentalNoiseSpline * continentalAmplitude;
	float height = continentalHeight;

	float oceanToBeach = std::min(std::max(continentalNoiseSpline * invBeachThreshold, 0.0f), 1.0f);

	// Erosion
	erosionNoise = (erosionNoise + 1.0f) * 0.5f;

	float erosionNormalized = MAX_FUNCTION(erosionNoise - erosionThreshold, 0.0f) * invOneMinusErosionThreshold;
	height -= erosionNormalized * erosionAmplitude * oceanToBeach;

	// Weirdness
	weirdnessNoise = weirdnessNoise + 1.0f; // [0, 2]
	float weirdnessHeight = (1.0f - std::abs(1.5f * weirdnessNoise - 2.0f)) * weirdnessAmplitude * oceanToBeach;
	height += weirdnessHeight;

	return height;
}

void TerrainGenerator::computeInitialHeightMap(int* heightMap, int chunkX, int chunkZ)
{
	PROFILE_SCOPE("Compute height map", ProfileCategory::TerrainGeneration);

	// Computing continental noise array
	float continentalNoiseArray[CHUNK_AREA];
	float erosionNoiseArray[CHUNK_AREA];
	float weirdnessNoiseArray[CHUNK_AREA];

	{
		NoiseParams params;
		params.frequency = 0.0001f;
		params.layerCount = 3;
		computeLayeredNoise_2D(continentalNoiseArray, chunkX, chunkZ, params);
	}
	{
		NoiseParams params;
		params.frequency = 0.01f;
		params.layerCount = 1;
		computeLayeredNoise_2D(erosionNoiseArray, chunkX, chunkZ, params);
	}
	{
		NoiseParams params;
		params.frequency = 0.005f;
		params.layerCount = 3;
		params.amplitudeFactor = 0.25f;
		params.frequencyFactor = 4.0f;
		computeLayeredNoise_2D(weirdnessNoiseArray, chunkX, chunkZ, params);
	}

	// Fill height map
	for (size_t i = 0; i < CHUNK_AREA; i++)
	{
		float continentalNoise = continentalNoiseArray[i];
		float erosionNoise = erosionNoiseArray[i];
		float weirdnessNoise = weirdnessNoiseArray[i];
		heightMap[i] = (int)calculateHeight(continentalNoise, erosionNoise, weirdnessNoise);
	}
}

int TerrainGenerator::computeMaxHeight(const int* heightMap)
{
	int maxHeight = std::numeric_limits<int>::min();
	for (size_t i = 0; i < CHUNK_AREA; i++)
	{
		if (heightMap[i] > maxHeight)
		{
			maxHeight = heightMap[i];
		}
	}
	return maxHeight;
}

void TerrainGenerator::computeLayeredNoise_2D(float* outArray, int chunkX, int chunkZ, const NoiseParams& params)
{
	std::fill(outArray, outArray + CHUNK_AREA, 0.0f);

	float layerAmplitude = 1.0f;
	float layerFrequency = params.frequency;
	float amplitudeSum = 0.0f;

	const int xStart = chunkX * CHUNK_SIZE;
	const int zStart = chunkZ * CHUNK_SIZE;

	float* tempNoiseArray = threadLocalData.resources->tempNoiseArray.data();
	const auto& simplexNoise = threadLocalData.resources->simplexNoise;
	for (int i = 0; i < params.layerCount; i++)
	{
		// X and Z are swapped because of FastNoise's coordinate system
		simplexNoise->GenUniformGrid2D(tempNoiseArray, zStart, xStart, CHUNK_SIZE, CHUNK_SIZE, layerFrequency, seed);
		for (int index = 0; index < CHUNK_AREA; index++)
		{
			outArray[index] += tempNoiseArray[index] * layerAmplitude;
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

void TerrainGenerator::computeLayeredNoise_3D(float* outArray, int chunkX, int chunkY, int chunkZ, const NoiseParams& params)
{
	std::fill(outArray, outArray + CHUNK_VOLUME, 0.0f);

	float layerAmplitude = 1.0f;
	float layerFrequency = params.frequency;

	float amplitudeSum = 0.0f;

	const int xStart = chunkX * CHUNK_SIZE;
	const int yStart = chunkY * CHUNK_SIZE;
	const int zStart = chunkZ * CHUNK_SIZE;

	float* tempNoiseArray = threadLocalData.resources->tempNoiseArray.data();
	const auto& simplexNoise = threadLocalData.resources->simplexNoise;
	for (int i = 0; i < params.layerCount; i++)
	{
		// X and Z are swapped because of FastNoise's coordinate system
		simplexNoise->GenUniformGrid3D(tempNoiseArray, zStart, yStart, xStart, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, layerFrequency, seed);
		for (int index = 0; index < CHUNK_VOLUME; index++)
		{
			outArray[index] += tempNoiseArray[index] * layerAmplitude;
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
// ThreadLocalData

TerrainGenerator::ThreadLocalData::ThreadLocalData() :
	resources(std::make_unique<Resources>())
{
	resources->simplexNoise = FastNoise::New<FastNoise::Simplex>();
}

//============================================================================