#pragma once
#include "Chunk/Metrics.h"

#include "Core/Simd.h"
#include "Core/MemoryAllocation/FixedArenaObjectPool.h"
#include "Core/Hashes/ivec2Hasher.h"

#include "robin_hood.h"
#include <mutex>
#include <array>

class TerrainGenerator;

class ChunkColumnData
{
	friend class TerrainGenerator;

	int X = 0, Z = 0; // Coordinates in chunk space

	std::array<int, CHUNK_AREA> heightMap{};

	std::atomic<bool> initialized{ false };

	std::atomic<uint16_t> referenceCount = 0;

	int maxHeight = 0;

	void init(int x, int z);
	void destroy();

	int* heightMapWrite() { return heightMap.data(); }

	void setToInitialized();
public:
	ChunkColumnData() = default;
	~ChunkColumnData() = default;

	const int* heightMapRead() const noexcept;

	int getMaxHeight() const noexcept { return maxHeight; }
};

class TerrainGenerator
{
	class ChunkColumnDataPool : public FixedArenaObjectPool<ChunkColumnData>
	{
		std::mutex poolMutex;
	public:
		ChunkColumnDataPool() = default;
		~ChunkColumnDataPool() = default;

		ChunkColumnDataPool(const ChunkColumnDataPool&) = delete;
		ChunkColumnDataPool& operator=(const ChunkColumnDataPool&) = delete;
		ChunkColumnDataPool(ChunkColumnDataPool&&) = delete;
		ChunkColumnDataPool& operator=(ChunkColumnDataPool&&) = delete;

		ChunkColumnData* acquire();

		void release(ChunkColumnData* chunkColumnData);
	};

	ChunkColumnDataPool chunkColumnDataPool;
	robin_hood::unordered_flat_map<glm::ivec2, ChunkColumnData*, ivec2Hasher> chunkColumnData;
	mutable std::mutex dataMutex; // Protects chunkColumnData map
	
	static int worldSeed;

	struct ThreadLocalData
	{
		alignas(SimdF::bytes) std::array<float, CHUNK_AREA> continentalNoiseArray;
		alignas(SimdF::bytes) std::array<float, CHUNK_AREA> erosionNoiseArray;
		alignas(SimdF::bytes) std::array<float, CHUNK_AREA> weirdnessNoiseArray;

		alignas(SimdF::bytes) std::array<float, CHUNK_VOLUME> caveWorleyNoiseArray{};
		alignas(SimdF::bytes) std::array<float, CHUNK_VOLUME> caveSimplexNoiseArray{};

		static constexpr int LOW_RES_SIZE = CHUNK_SIZE / 2 + 1;
		static constexpr int LOW_RES_VOLUME = CHUNK_VOLUME;// LOW_RES_SIZE* LOW_RES_SIZE* LOW_RES_SIZE;
		alignas(SimdF::bytes) std::array<float, LOW_RES_VOLUME> noiseArrayForUpscaling{};
	};

	static thread_local ThreadLocalData threadLocalData;

	TerrainGenerator();
	~TerrainGenerator() = default;
public:
	TerrainGenerator(const TerrainGenerator& other) = delete;
	TerrainGenerator& operator=(const TerrainGenerator& other) = delete;
	TerrainGenerator(TerrainGenerator&& other) = delete;
	TerrainGenerator& operator=(TerrainGenerator&& other) = delete;

	static TerrainGenerator& getInstance();

	const ChunkColumnData* loadChunkColumnData(int chunkX, int chunkZ);
	const ChunkColumnData* getChunkColumnData(int chunkX, int chunkZ);
	void unloadChunkColumnData(int chunkX, int chunkZ);

	void computeCaveMask(bool* outArray, int chunkX, int chunkY, int chunkZ) const;

	// Debug
	size_t getChunkColumnDataCount() const;
private:
	void initChunkColumnData(ChunkColumnData* column, int X, int Z);

	static float calculateHeight(float continentalNoise, float erosionNoise, float weirdnessNoise);
	static void computeInitialHeightMap(int* heightMap, int chunkX, int chunkZ);
	static int computeMaxHeight(const int* heightMap);

	struct NoiseParams
	{
		float frequency = 1.0f;
		int layerCount = 1;
		float lacunarity = 2.0f;
	};

	static void computeLayeredNoise_2D(float* outArray, int chunkX, int chunkZ, const NoiseParams& params);

	static void computeLayeredNoise_3D(float* outArray, int chunkX, int chunkY, int chunkZ, const NoiseParams& params);

	static void computeLayeredNoise_3D_Upscaled(float* outArray, int chunkX, int chunkY, int chunkZ, const NoiseParams& params, int upscaleFactor);
};

