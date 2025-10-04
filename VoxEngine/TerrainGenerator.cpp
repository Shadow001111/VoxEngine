#include "TerrainGenerator.h"

#include "Profiler.h"

#include <iostream>

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
}

const int* ChunkColumnData::getHeightReadPointer() const
{
	return heightMap;
}

int* ChunkColumnData::getHeightWritePointer()
{
	return heightMap;
}

//============================================================================
//ChunkColumnDataPool

std::unique_ptr<ChunkColumnData> TerrainGenerator::ChunkColumnDataPool::acquire()
{
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
	pool.push_back(std::move(chunkColumnData));
}

//============================================================================
//TerrainGenerator

TerrainGenerator& TerrainGenerator::getInstance()
{
	static TerrainGenerator instance;
	return instance;
}

const ChunkColumnData* TerrainGenerator::loadChunkColumnData(int x, int z)
{
	PROFILE_SCOPE("Load chunk column data");

	// Check if column already exists
	Int2 pos(x, z);
	auto it = chunkColumnData.find(pos);
	if (it != chunkColumnData.end())
	{
		// TODO: It's not being called!
		std::cout << "Column already exists" << std::endl;
		return it->second.get();
	}

	// Create column
	std::unique_ptr<ChunkColumnData> column = chunkColumnDataPool.acquire();

	// Move column into the map
	auto inserted = chunkColumnData.insert(std::make_pair(pos, std::move(column)));

	// Init column
	ChunkColumnData* columnPtr = inserted.first->second.get();
	initChunkColumnData(columnPtr, x, z);

	return columnPtr;
}

void TerrainGenerator::unloadChunkColumnData(int x, int z)
{
	PROFILE_SCOPE("Unload chunk column data");

	// Check if column exists
	Int2 pos(x, z);
	auto it = chunkColumnData.find(pos);
	if (it == chunkColumnData.end())
	{
		// Nothing to unload
		return;
	}

	// Release the unique_ptr back to the pool before erasing
	chunkColumnDataPool.release(std::move(it->second));

	// Remove the entry from the map
	chunkColumnData.erase(it);
}

void TerrainGenerator::initChunkColumnData(ChunkColumnData* column, int x, int z)
{
	PROFILE_SCOPE("Init chunk column data");

	column->init(x, z);

	int* heightMap = column->getHeightWritePointer();
	for (int i = 0; i < CHUNK_AREA; i++)
	{
		heightMap[i] = 0; // Flat terrain at height 0
	}
}

//============================================================================