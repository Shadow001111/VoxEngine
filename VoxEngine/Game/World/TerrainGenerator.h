#pragma once
#include "Chunk/Metrics.h"

#define FASTNOISE_STATIC_LIB
#include "FastNoise/FastNoise.h"

#include "Core/MemoryAllocation/FixedArenaObjectPool.h"
#include "Core/Hashes/ivec2Hasher.h"

#include "robin_hood.h"
#include <mutex>
#include <array>
#include <condition_variable>

class ChunkColumnData
{
	int X = 0, Z = 0; // Coordinates in chunk space

	std::array<int, CHUNK_AREA> heightMap{};

	mutable std::mutex readDataMutex; // Prevents chunks from reading heightMap until it's built
	mutable std::condition_variable readDataCV;

	bool initialized = false;
public:
	std::atomic<int32_t> referenceCount = 0;

	ChunkColumnData() = default;
	~ChunkColumnData() = default;

	void init(int x, int z);
	void destroy();

	const int* heightMapRead() const;
	int* heightMapWrite() { return heightMap.data(); }

	void setToInitialized();
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
	
	static int seed;

	struct ThreadLocalData
	{
		struct Resources
		{
			FastNoise::SmartNode<FastNoise::Simplex> simplexNoise;
			std::array<float, CHUNK_VOLUME> tempNoiseArray{};
			std::array<float, CHUNK_VOLUME> caveNoiseArray{};
		};

		std::unique_ptr<Resources> resources;

		ThreadLocalData();
	};

	static thread_local ThreadLocalData threadLocalData;

	TerrainGenerator() = default;
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

	struct NoiseParams
	{
		float frequency = 1.0f;

		int layerCount = 1;
		float amplitudeFactor = 0.5f;
		float frequencyFactor = 2.0f;
	};

	static void computeLayeredNoise_2D(float* outArray, int chunkX, int chunkZ, const NoiseParams& params);

	static void computeLayeredNoise_3D(float* outArray, int chunkX, int chunkY, int chunkZ, const NoiseParams& params);
};

