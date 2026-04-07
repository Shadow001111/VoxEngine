#include "TerrainGenerator.h"

#include "Core/Profiler.h"

#include "NoiseLib/Perlin.h"
#include "NoiseLib/Simplex.h"
#include "NoiseLib/Worley.h"

//============================================================================
static float continentalSpline(float x)
{
	return x;
}

static float lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

static float trilerp(float c000, float c100, float c010, float c110, float c001, float c101, float c011, float c111, float tx, float ty, float tz)
{
	float x00 = lerp(c000, c100, tx);
	float x10 = lerp(c010, c110, tx);
	float x01 = lerp(c001, c101, tx);
	float x11 = lerp(c011, c111, tx);
	float y0 = lerp(x00, x10, ty);
	float y1 = lerp(x01, x11, ty);
	return lerp(y0, y1, tz);
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

const int* ChunkColumnData::heightMapRead() const noexcept
{
	initialized.wait(false, std::memory_order_acquire); // Wait until 'initialized' won't be false
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

using TerrainPerlinNoiseGenerator = NoiseLib::Base::BaseNoiseGenerator<
	NoiseLib::Perlin::scalar2D,
	NoiseLib::Perlin::simd2D,
	NoiseLib::Perlin::scalar3D,
	NoiseLib::Perlin::simd3D,
	false, // Not seamless
	true,  // Aligned data
	false  // No tail
>;

using CaveSimplexNoiseGenerator = NoiseLib::Base::BaseNoiseGenerator<
	NoiseLib::Simplex::scalar2D,
	NoiseLib::Simplex::simd2D,
	NoiseLib::Simplex::scalar3D,
	NoiseLib::Simplex::simd3D,
	false, // Not seamless
	true,  // Aligned data
	false  // No tail
>;

using CaveWorleyUpscaledNoiseGenerator = NoiseLib::Base::BaseNoiseGenerator<
	NoiseLib::Worley::scalar2D,
	NoiseLib::Worley::simd2D,
	NoiseLib::Worley::scalar3D,
	NoiseLib::Worley::simd3D,
	false, // Not seamless
	false, // Unaligned data
	true   // Has tail
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
	float* caveWorleyNoiseArray = threadLocalData.caveWorleyNoiseArray.data();
	float* caveSimplexNoiseArray = threadLocalData.caveSimplexNoiseArray.data();

	constexpr float caveNoiseFrequency = 0.6f;

	{
		PROFILE_SCOPE("Cave mask: compute worley noise", ProfileCategory::TerrainGeneration);

		NoiseParams params;
		params.frequency = caveNoiseFrequency;
		params.layerCount = 2;
		computeLayeredNoise_3D_Upscaled(caveWorleyNoiseArray, chunkX, chunkY, chunkZ, params, 4);
	}

	{
		PROFILE_SCOPE("Cave mask: compute simplex noise", ProfileCategory::TerrainGeneration);

		NoiseParams params;
		params.frequency = caveNoiseFrequency * 2.0f;
		params.layerCount = 1;
		computeLayeredNoise_3D(caveSimplexNoiseArray, chunkX, chunkY, chunkZ, params);
	}

	// Simplex noise is used to smooth out upscaled worley noise, making caves less blocky
	{
		PROFILE_SCOPE("Cave mask: combine noises", ProfileCategory::TerrainGeneration);

		constexpr float minThreshold = 0.38f;
		constexpr float maxThreshold = 0.45f;

		constexpr float minY = -100.0f;
		constexpr float maxY = -20.0f;

		constexpr float bias = 0.02f;

		for (int i = 0; i < CHUNK_VOLUME; i++)
		{
			float globalY = (chunkY * CHUNK_SIZE + (i / CHUNK_AREA));

			float gradient = std::clamp((globalY - minY) / (maxY - minY), 0.0f, 1.0f);
			float threshold = lerp(minThreshold, maxThreshold, gradient);

			float simplexNoise = caveSimplexNoiseArray[i];
			simplexNoise = (simplexNoise + 1.0f) * 0.5f; // [0, 1]

			float worleyNoise = caveWorleyNoiseArray[i];

			float value = worleyNoise + simplexNoise * (1.0f - gradient) * -0.1f + bias;

			outArray[i] = value > threshold;
		}
//		static_assert((CHUNK_VOLUME % SimdF::lanes) == 0, "Add tail logic!");
//
//		const SimdF absMask = SimdI::fill_lanes_with_value(0x7fffffff).as_float();
//		const SimdF threshold = SimdF::fill_lanes_with_value(0.1f);
//		for (int i = 0; i + SimdF::lanes <= CHUNK_VOLUME; i += SimdF::lanes)
//		{
//			// Load values
//			SimdF values = SimdF::load(caveNoiseArray + i);
//
//			// Convert values to absolute (positive)
//			values &= absMask;
//
//			// Create a comparison mask
//			SimdI comparisons = (values < threshold).as_int32();
//
//			// Pack bools: 32-bit -> 16-bit -> 8-bit
//#ifdef SIMD_AVX2
//			{
//				Simd128I low = SimdI::extract_int_128<0>(comparisons);
//				Simd128I high = SimdI::extract_int_128<1>(comparisons);
//
//				low = Simd128I::narrow_saturate_32_to_16(low, high);
//				low = Simd128I::narrow_saturate_16_to_8(low, low);
//
//				// Store 8 bools at once
//				low.store_lower_int_64(reinterpret_cast<int32_t*>(outArray + i));
//			}
//#elifdef SIMD_SSE
//			{
//				comparisons = Simd128I::narrow_saturate_32_to_16(comparisons, comparisons);
//				comparisons = Simd128I::narrow_saturate_16_to_8(comparisons, comparisons);
//
//				// Store 4 bools at once
//				*((int32_t*)(outArray + i)) = comparisons.get_least_significant_int_32();
//			}
//#endif
//		}
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
	TerrainPerlinNoiseGenerator::genLayered2D
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
	TerrainPerlinNoiseGenerator::genLayered3D
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

void TerrainGenerator::computeLayeredNoise_3D_Upscaled(float* outArray, int chunkX, int chunkY, int chunkZ, const NoiseParams& params, int upscaleFactor)
{
	upscaleFactor = std::max(upscaleFactor, 2); // Safety?

	float* lowResArray = threadLocalData.noiseArrayForUpscaling.data();

	// Generate noise at lower resolution
	const int lowResSize = CHUNK_SIZE / upscaleFactor + 1; // +1 to have an extra row/column/layer for interpolation at the borders
	const int lowResArea = lowResSize * lowResSize;
	CaveWorleyUpscaledNoiseGenerator::genLayered3D
	(
		lowResArray,
		worldSeed,
		{ lowResSize, lowResSize, lowResSize },
		1.0f / params.frequency,
		{ chunkZ * lowResSize, chunkY * lowResSize, chunkX * lowResSize },
		params.layerCount,
		params.lacunarity
	);

	auto getNormalIndex = [](int x, int y, int z) {
		return x * CHUNK_AREA + y * CHUNK_SIZE + z;
		};

	auto getLowResIndex = [lowResArea, lowResSize](int x, int y, int z) {
		return x * lowResArea + y * lowResSize + z;
		};

	// Upscale using trilinear interpolation
	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				// Calculate the corresponding low-res coordinates
				float x0f = x / (float)upscaleFactor;
				float y0f = y / (float)upscaleFactor;
				float z0f = z / (float)upscaleFactor;

				int x0 = std::floor(x0f);
				int y0 = std::floor(y0f);
				int z0 = std::floor(z0f);

				// Calculate next cell coordinates, clamping to the maximum index
				int x1 = std::min(x0 + 1, lowResSize - 1);
				int y1 = std::min(y0 + 1, lowResSize - 1);
				int z1 = std::min(z0 + 1, lowResSize - 1);

				// Calculate corner indices
				int index000 = getLowResIndex(x0, y0, z0);
				int index100 = getLowResIndex(x1, y0, z0);
				int index010 = getLowResIndex(x0, y1, z0);
				int index110 = getLowResIndex(x1, y1, z0);
				int index001 = getLowResIndex(x0, y0, z1);
				int index101 = getLowResIndex(x1, y0, z1);
				int index011 = getLowResIndex(x0, y1, z1);
				int index111 = getLowResIndex(x1, y1, z1);

				// Calculate interpolation weights
				float tx = x0f - x0;
				float ty = y0f - y0;
				float tz = z0f - z0;

				// Fetch the 8 corner values from the low-res array
				float c000 = lowResArray[index000];
				float c100 = lowResArray[index100];
				float c010 = lowResArray[index010];
				float c110 = lowResArray[index110];
				float c001 = lowResArray[index001];
				float c101 = lowResArray[index101];
				float c011 = lowResArray[index011];
				float c111 = lowResArray[index111];

				// Perform trilinear interpolation
				outArray[getNormalIndex(x, y, z)] = trilerp(
					c000, c100, c010, c110,
					c001, c101, c011, c111,
					tx, ty, tz
				);
			}
		}
	}
}

//============================================================================