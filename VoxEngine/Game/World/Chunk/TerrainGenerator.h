#pragma once
#include "Metrics.h"

#define FASTNOISE_STATIC_LIB
#include "FastNoise/FastNoise.h"

#include "Core/Int2.h"

#include <unordered_map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>

class ChunkColumnData
{
	int X, Z; // Coordinates in chunk space

	int heightMap[CHUNK_AREA];

	bool initialized = false;
public:
	std::atomic<int32_t> referenceCount = 0;
private:
	mutable std::mutex readDataMutex; // Prevents chunks from reading heightMap until it's built
	mutable std::condition_variable readDataCV;
public:
	ChunkColumnData();
	~ChunkColumnData();

	void init(int x, int z);
	void destroy();

	const int* heightMapRead() const;
	int* heightMapWrite();

	void setToInitialized();
};

class TerrainGenerator
{
	class ChunkColumnDataPool
	{
		std::vector<std::unique_ptr<ChunkColumnData>> pool;
		std::mutex poolMutex;
	public:
		ChunkColumnDataPool() = default;
		~ChunkColumnDataPool() = default;

		ChunkColumnDataPool(const ChunkColumnDataPool&) = delete;
		ChunkColumnDataPool& operator=(const ChunkColumnDataPool&) = delete;
		ChunkColumnDataPool(ChunkColumnDataPool&&) = delete;
		ChunkColumnDataPool& operator=(ChunkColumnDataPool&&) = delete;

		std::unique_ptr<ChunkColumnData> acquire();

		void release(std::unique_ptr<ChunkColumnData> chunkColumnData);
	};

	ChunkColumnDataPool chunkColumnDataPool;
	std::unordered_map<Int2, std::unique_ptr<ChunkColumnData>, Int2Hasher> chunkColumnData;
	mutable std::mutex dataMutex; // Protects chunkColumnData map
	
	static int seed;
	static thread_local FastNoise::SmartNode<FastNoise::Simplex> simplexNoise;
	static thread_local std::vector<float> internalLayeredNoiseArray;
	static thread_local std::vector<float> caveNoiseArray;
public:
	TerrainGenerator() = default;
	~TerrainGenerator() = default;

	TerrainGenerator(const TerrainGenerator& other) = delete;
	TerrainGenerator& operator=(const TerrainGenerator& other) = delete;
	TerrainGenerator(TerrainGenerator&& other) = delete;
	TerrainGenerator& operator=(TerrainGenerator&& other) = delete;

	static TerrainGenerator& getInstance();

	static void initThread();

	const ChunkColumnData* loadChunkColumnData(int chunkX, int chunkZ);
	const ChunkColumnData* getChunkColumnData(int chunkX, int chunkZ);
	void unloadChunkColumnData(int chunkX, int chunkZ);

	void computeCaveMask(bool* outArray, int chunkX, int chunkY, int chunkZ);

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

	static void computeNoise_2D(float* outArray, int chunkX, int chunkZ, float frequency);
	static void computeLayeredNoise_2D(float* outArray, int chunkX, int chunkZ, const NoiseParams& params);

	static void computeNoise_3D(float* outArray, int chunkX, int chunkY, int chunkZ, float frequency);
	static void computeLayeredNoise_3D(float* outArray, int chunkX, int chunkY, int chunkZ, const NoiseParams& params);
};

