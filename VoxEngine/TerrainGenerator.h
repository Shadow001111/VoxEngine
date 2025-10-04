#pragma once
#include "Metrics.h"

#include "Int2.h"

#include <unordered_map>
#include <memory>

// TODO: Keep track of usage so we can delete safely
struct ChunkColumnData
{
	int X, Z; // Coordinates in chunk space

	int heightMap[CHUNK_AREA];
public:
	ChunkColumnData();
	~ChunkColumnData();

	void init(int x, int z);
	void destroy();

	const int* getHeightReadPointer() const;
	int* getHeightWritePointer();
};

// TODO: Implement cleaning up unneeded columns
class TerrainGenerator
{
	class ChunkColumnDataPool
	{
		std::vector<std::unique_ptr<ChunkColumnData>> pool;
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
public:
	TerrainGenerator() = default;
	~TerrainGenerator() = default;

	TerrainGenerator(const TerrainGenerator& other) = delete;
	TerrainGenerator& operator=(const TerrainGenerator& other) = delete;
	TerrainGenerator(TerrainGenerator&& other) = delete;
	TerrainGenerator& operator=(TerrainGenerator&& other) = delete;

	static TerrainGenerator& getInstance();

	const ChunkColumnData* loadChunkColumnData(int x, int z);
	void unloadChunkColumnData(int x, int z);
private:
	void initChunkColumnData(ChunkColumnData* column, int x, int z);
};

