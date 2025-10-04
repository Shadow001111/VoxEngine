#include "TerrainGenerator.h"

#include "Profiler.h"

#include <iostream>
#include <cmath>

//============================================================================
//ChunkColumnData

ChunkColumnData::ChunkColumnData() :
	referenceCount(0)
{
}

ChunkColumnData::~ChunkColumnData()
{
}

void ChunkColumnData::init(int x, int z)
{
	X = x; Z = z;
	referenceCount = 0;
}

void ChunkColumnData::destroy()
{
	referenceCount = 0;
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
		it->second->referenceCount++;
		return it->second.get();
	}

	// Create column
	std::unique_ptr<ChunkColumnData> column = chunkColumnDataPool.acquire();

	// Move column into the map
	auto inserted = chunkColumnData.insert(std::make_pair(pos, std::move(column)));

	// Init column
	ChunkColumnData* columnPtr = inserted.first->second.get();
	initChunkColumnData(columnPtr, x, z);
	columnPtr->referenceCount = 1;

	return columnPtr;
}

void TerrainGenerator::releaseChunkColumnData(int x, int z)
{
	PROFILE_SCOPE("Release chunk column data");

	Int2 pos(x, z);
	auto it = chunkColumnData.find(pos);
	if (it == chunkColumnData.end())
	{
		return;
	}

	// Decrement reference count
	it->second->referenceCount--;

	// If no more references, unload the column
	if (it->second->referenceCount <= 0)
	{
		chunkColumnDataPool.release(std::move(it->second));
		chunkColumnData.erase(it);
	}
}

size_t TerrainGenerator::getChunkColumnDataCount() const
{
	return chunkColumnData.size();
}

void TerrainGenerator::initChunkColumnData(ChunkColumnData* column, int X, int Z)
{
	PROFILE_SCOPE("Init chunk column data");

	column->init(X, Z);

	int* heightMap = column->getHeightWritePointer();
	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		int globalX = x + X * CHUNK_SIZE;
		for (int z = 0; z < CHUNK_SIZE; z++)
		{
			int globalZ = z + Z * CHUNK_SIZE;

			float v = (sinf(globalX * 0.1f) + sinf(globalZ * 0.1f)) * 0.5f;

			int height = v * 10.0f;

			heightMap[z + x * CHUNK_SIZE] = height;
		}
	}
}

//============================================================================