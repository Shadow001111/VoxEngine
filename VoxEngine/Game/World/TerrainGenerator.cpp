#include "TerrainGenerator.h"

#include "Core/Profiler.h"

#include "NoiseLib/Perlin.h"
#include "NoiseLib/Simplex.h"

//============================================================================
static float continentalSpline(float x)
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

int TerrainGenerator::worldSeed = 0;
thread_local TerrainGenerator::ThreadLocalData TerrainGenerator::threadLocalData;

using CombinedNoiseGenerator = NoiseLib::Base::BaseNoiseGenerator<
	NoiseLib::Perlin::scalar2D,
	NoiseLib::Perlin::simd2D,
	NoiseLib::Simplex::scalar3D,
	NoiseLib::Simplex::simd3D,
	false, // Not seamless
	true,  // Aligned data
	false  // No tail
>;

TerrainGenerator::TerrainGenerator()
{
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
	float* caveNoiseArray = threadLocalData.caveNoiseArray.data();

	{
		PROFILE_SCOPE("Cave mask: compute noises", ProfileCategory::TerrainGeneration);

		NoiseParams params;
		params.frequency = 0.2f;
		params.layerCount = 3;
		computeLayeredNoise_3D(caveNoiseArray, chunkX, chunkY, chunkZ, params);
	}

	{
		PROFILE_SCOPE("Cave mask: combine noises", ProfileCategory::TerrainGeneration);

		//for (int i = 0; i < CHUNK_VOLUME; i++)
		//{
		//	outArray[i] = std::abs(caveNoiseArray[i]) < 0.1f;
		//}

		static_assert((CHUNK_VOLUME % SimdF::lanes) == 0, "Add tail logic!");

		const SimdF absMask = SimdI::fill_lanes_with_value(0x7fffffff).as_float();
		const SimdF threshold = SimdF::fill_lanes_with_value(0.1f);
		for (int i = 0; i + SimdF::lanes <= CHUNK_VOLUME; i += SimdF::lanes)
		{
			// Load values
			SimdF values = SimdF::load(caveNoiseArray + i);

			// Convert values to absolute (positive)
			values &= absMask;

			// Create a comparison mask
			SimdI comparisons = (values < threshold).as_int32();

			// Pack bools: 32-bit -> 16-bit -> 8-bit
#ifdef SIMD_AVX2
			{
				Simd128I low = SimdI::extract_int_128<0>(comparisons);
				Simd128I high = SimdI::extract_int_128<1>(comparisons);

				low = Simd128I::narrow_saturate_32_to_16(low, high);
				low = Simd128I::narrow_saturate_16_to_8(low, low);

				// Store 8 bools at once
				low.store_lower_int_64(reinterpret_cast<int32_t*>(outArray + i));
			}
#elifdef SIMD_SSE
			{
				comparisons = Simd128I::narrow_saturate_32_to_16(comparisons, comparisons);
				comparisons = Simd128I::narrow_saturate_16_to_8(comparisons, comparisons);

				// Store 4 bools at once
				*((int32_t*)(outArray + i)) = comparisons.get_least_significant_int_32();
			}
#endif
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
	constexpr float erosionAmplitude = 20.0f;
	constexpr float erosionThreshold = 0.65f;
	constexpr float weirdnessAmplitude = 20.0f;
	constexpr float beachThreshold = 0.2f / continentalAmplitude;

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

	float erosionNormalized = std::max(erosionNoise - erosionThreshold, 0.0f) * invOneMinusErosionThreshold;
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

	// Compute noise arrays
	{
		NoiseParams params;
		params.frequency = 0.001f;
		params.layerCount = 3;
		computeLayeredNoise_2D(threadLocalData.continentalNoiseArray.data(), chunkX, chunkZ, params);
	}
	{
		NoiseParams params;
		params.frequency = 0.2f;
		params.layerCount = 1;
		computeLayeredNoise_2D(threadLocalData.erosionNoiseArray.data(), chunkX, chunkZ, params);
	}
	{
		NoiseParams params;
		params.frequency = 0.1f;
		params.layerCount = 3;
		params.lacunarity = 4.0f;
		computeLayeredNoise_2D(threadLocalData.weirdnessNoiseArray.data(), chunkX, chunkZ, params);
	}

	// Fill height map
	for (size_t i = 0; i < CHUNK_AREA; i++)
	{
		float continentalNoise = threadLocalData.continentalNoiseArray[i];
		float erosionNoise = threadLocalData.erosionNoiseArray[i];
		float weirdnessNoise = threadLocalData.weirdnessNoiseArray[i];
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
	CombinedNoiseGenerator::genLayered2D
	(
		outArray,
		worldSeed,
		{ CHUNK_SIZE, CHUNK_SIZE },
		1.0f / params.frequency,
		glm::vec2(chunkZ, chunkX) * (float)CHUNK_SIZE,
		params.layerCount,
		params.lacunarity
	);
}

void TerrainGenerator::computeLayeredNoise_3D(float* outArray, int chunkX, int chunkY, int chunkZ, const NoiseParams& params)
{
	CombinedNoiseGenerator::genLayered3D
	(
		outArray,
		worldSeed,
		{ CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE },
		1.0f / params.frequency,
		{ chunkZ * CHUNK_SIZE, chunkY * CHUNK_SIZE, chunkX * CHUNK_SIZE },
		params.layerCount,
		params.lacunarity
	);
}

//============================================================================