#pragma once
#include "Metrics.h"

#include "Int2.h"

#include <unordered_map>
#include <memory>
#include <mutex>

struct ChunkColumnData
{
	int X, Z; // Coordinates in chunk space
	uint32_t referenceCount;

	int heightMap[CHUNK_AREA];
public:
	ChunkColumnData();
	~ChunkColumnData();

	void init(int x, int z);
	void destroy();
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
public:
	TerrainGenerator() = default;
	~TerrainGenerator() = default;

	TerrainGenerator(const TerrainGenerator& other) = delete;
	TerrainGenerator& operator=(const TerrainGenerator& other) = delete;
	TerrainGenerator(TerrainGenerator&& other) = delete;
	TerrainGenerator& operator=(TerrainGenerator&& other) = delete;

	static TerrainGenerator& getInstance();

	const ChunkColumnData* loadChunkColumnData(int x, int z);
	void releaseChunkColumnData(int x, int z);

	// Debug
	size_t getChunkColumnDataCount() const;
private:
	void initChunkColumnData(ChunkColumnData* column, int X, int Z);
};

