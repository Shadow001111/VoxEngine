#include "ChunkRegion.h"
#include "../Chunk.h"

const size_t ChunkRegion::getChunkCount() const
{
	size_t count = 0;
	for (const auto* chunk : chunks)
	{
		if (chunk != nullptr)
		{
			count++;
		}
	}
	return count;
}

glm::ivec3 ChunkRegionManager::getRegionPosition(const glm::ivec3& chunkPosition) const
{
	return chunkPosition >> CHUNK_REGION_SIZE_LOG2;
}

size_t ChunkRegionManager::getChunkIndexInRegion(const glm::ivec3& chunkPosition) const
{
	glm::ivec3 chunkPosInRegion = chunkPosition & CHUNK_REGION_LOWER_BITS_MASK;
	return (chunkPosInRegion.x << (CHUNK_REGION_SIZE_LOG2 << 1)) | (chunkPosInRegion.y << CHUNK_REGION_SIZE_LOG2) | chunkPosInRegion.z;
}

void ChunkRegionManager::addChunk(Chunk* chunk)
{
	// Get region position
	glm::ivec3 regionPos = getRegionPosition(chunk->getPosition());

	// If region doesn't exist, create it
	//std::lock_guard<std::mutex> lock(regionsMutex);

	ChunkRegion* region;
	if (regions.contains(regionPos))
	{
		region = regions[regionPos];
	}
	else
	{
		// Aquire region from pool
		region = regionPool.acquire();

		regions[regionPos] = region;
	}

	// Get chunk index in region
	size_t index = getChunkIndexInRegion(chunk->getPosition());

	// Add chunk to region
	region->chunks[index] = chunk;
}

void ChunkRegionManager::removeChunk(Chunk* chunk)
{
	// Get region position
	glm::ivec3 regionPos = getRegionPosition(chunk->getPosition());

	// Get region
	//std::lock_guard<std::mutex> lock(regionsMutex);

	if (!regions.contains(regionPos))
	{
		return;
	}

	ChunkRegion* region = regions[regionPos];

	// Get chunk index in region
	size_t index = getChunkIndexInRegion(chunk->getPosition());

	// Remove chunk from region
	region->chunks[index] = nullptr;

	// TODO: If region is empty, remove it from map
	// Return region to pool if it's empty
}

size_t ChunkRegionManager::getRegionCount() const
{
	//std::lock_guard<std::mutex> lock(regionsMutex);
	return regions.size();
}

size_t ChunkRegionManager::getChunkCount() const
{
	//std::lock_guard<std::mutex> lock(regionsMutex);

	size_t count = 0;
	for (const auto& [_, region] : regions)
	{
		count += region->getChunkCount();
	}
	return count;
}
