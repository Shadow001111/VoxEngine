#include "TerrainGenerator.h"

#include "Core/Profiler.h"

#include <intrin.h>

//============================================================================
static float continentalSpline(float x)
{
	return x;
}

static float sumOfGeomtricSeries(float firstTerm, float commonRatio, int numberOfTerms)
{
	if (commonRatio == 1.0f) return firstTerm * numberOfTerms;
	return firstTerm * (1.0f - powf(commonRatio, numberOfTerms)) / (1.0f - commonRatio);
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
	referenceCount.store(0, std::memory_order_relaxed);
	initialized.store(false, std::memory_order_relaxed);
}

const int* ChunkColumnData::heightMapRead() const
{
	initialized.wait(false, std::memory_order_acquire);
	return heightMap.data();
}

void ChunkColumnData::setToInitialized()
{
	initialized.store(true, std::memory_order_release);
	initialized.notify_all();
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
FastNoise::SmartNode<FastNoise::Simplex> TerrainGenerator::simplexNoise;

TerrainGenerator::TerrainGenerator()
{
	simplexNoise = FastNoise::New<FastNoise::Simplex>();
}

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

		//for (int i = 0; i < CHUNK_VOLUME; i++)
		//{
		//	float value = caveNoiseArray[i];
		//	value = ABS_FUNCTION(value);
		//
		//	outArray[i] = value < 0.1f;
		//}

#ifdef __AVX2__
		// AVX/AVX2 implementation
		{
			const __m256 abs_mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff)); // sign-strip mask
			const __m256 threshold = _mm256_set1_ps(0.1f);

			for (int i = 0; i < CHUNK_VOLUME; i += 8)
			{
				__m256 values = _mm256_load_ps(&caveNoiseArray[i]);          // load 8 floats
				__m256 absoluteValues = _mm256_and_ps(values, abs_mask);      // abs(x)
				__m256 cmpResult = _mm256_cmp_ps(absoluteValues, threshold, _CMP_LT_OQ);  // abs(x) < 0.1f
				__m256i comparisons = _mm256_castps_si256(cmpResult);

				// Pack bools: 32-bit -> 16-bit -> 8-bit
				// AVX2 packs operate within 128-bit lanes, so we need to account for that
				__m128i lo = _mm256_extracti128_si256(comparisons, 0);
				__m128i hi = _mm256_extracti128_si256(comparisons, 1);

				lo = _mm_packs_epi32(lo, hi);    // 8x 32-bit -> 8x 16-bit (SSE2, fits in 128-bit)
				lo = _mm_packs_epi16(lo, lo);    // 8x 16-bit -> 8x  8-bit (result in lower 64 bits)

				// Store 8 bools at once
				*((int64_t*)(outArray + i)) = _mm_cvtsi128_si64(lo);
			}
		}
		// I don't wanna do AVX
#else
		// SSE implementation
		{
			const __m128 abs_mask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)); // sign-strip mask
			const __m128 threshold = _mm_set1_ps(0.1f);
			//const __m128i true_mask = _mm_set1_epi8(1);

			for (int i = 0; i < CHUNK_VOLUME; i += 4)
			{
				__m128 values = _mm_load_ps(&caveNoiseArray[i]);         // load 4 floats
				__m128 absoluteValues = _mm_and_ps(values, abs_mask);     // abs(x)
				__m128i comparisons = _mm_castps_si128(_mm_cmplt_ps(absoluteValues, threshold));     // abs(x) < 0.1f

				// Pack bools
				comparisons = _mm_packs_epi32(comparisons, comparisons);
				comparisons = _mm_packs_epi16(comparisons, comparisons);
				//comparisons = _mm_and_si128(comparisons, true_mask); // Convert to 0 or 1, but it's not necessary for storing in bool array

				// Store 4 bools at once
				*((int32_t*)(outArray + i)) = _mm_cvtsi128_si32(comparisons);
			}
		}
#endif
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
	alignas(SIMD_ALIGNMENT) float continentalNoiseArray[CHUNK_AREA];
	alignas(SIMD_ALIGNMENT) float erosionNoiseArray[CHUNK_AREA];
	alignas(SIMD_ALIGNMENT) float weirdnessNoiseArray[CHUNK_AREA];

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
	// Zero array
	std::fill(outArray, outArray + CHUNK_AREA, 0.0f);

	// Calculate sum
	const float maxSum = sumOfGeomtricSeries(1.0f, params.amplitudeFactor, params.layerCount);

	// Set initial layer params
	float layerAmplitude = 1.0f / maxSum;
	float layerFrequency = params.frequency;

	// Set start chunk coords
	const int xStart = chunkX * CHUNK_SIZE;
	const int zStart = chunkZ * CHUNK_SIZE;

	// Generate noise
	float* tempNoiseArray = threadLocalData.resources->tempNoiseArray.data();
	for (int i = 0; i < params.layerCount; i++)
	{
		// X and Z are swapped because of FastNoise's coordinate system
		simplexNoise->GenUniformGrid2D(tempNoiseArray, zStart, xStart, CHUNK_SIZE, CHUNK_SIZE, layerFrequency, seed);

		for (int index = 0; index < CHUNK_AREA; index++)
		{
			outArray[index] += tempNoiseArray[index] * layerAmplitude;
		}

		layerAmplitude *= params.amplitudeFactor;
		layerFrequency *= params.frequencyFactor;
	}
}

void TerrainGenerator::computeLayeredNoise_3D(float* outArray, int chunkX, int chunkY, int chunkZ, const NoiseParams& params)
{
	// Zero array
	std::fill(outArray, outArray + CHUNK_VOLUME, 0.0f);

	// Calculate sum
	const float maxSum = sumOfGeomtricSeries(1.0f, params.amplitudeFactor, params.layerCount);

	// Set initial layer params
	float layerAmplitude = 1.0f / maxSum;
	float layerFrequency = params.frequency;

	// Set start chunk coords
	const int xStart = chunkX * CHUNK_SIZE;
	const int yStart = chunkY * CHUNK_SIZE;
	const int zStart = chunkZ * CHUNK_SIZE;

	// Generate noise
	float* tempNoiseArray = threadLocalData.resources->tempNoiseArray.data();
	for (int i = 0; i < params.layerCount; i++)
	{
		// X and Z are swapped because of FastNoise's coordinate system
		simplexNoise->GenUniformGrid3D(tempNoiseArray, zStart, yStart, xStart, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, layerFrequency, seed);

		for (int index = 0; index < CHUNK_VOLUME; index++)
		{
			outArray[index] += tempNoiseArray[index] * layerAmplitude;
		}

		layerAmplitude *= params.amplitudeFactor;
		layerFrequency *= params.frequencyFactor;
	}
}

//============================================================================
// ThreadLocalData

TerrainGenerator::ThreadLocalData::ThreadLocalData() :
	resources(std::make_unique<Resources>())
{
}

//============================================================================