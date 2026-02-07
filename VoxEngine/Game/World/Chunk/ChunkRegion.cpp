#include "ChunkRegion.h"
#include "../Chunk.h"
#include "Core/Assert.h"

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

	// Check if region already exists
	//std::lock_guard<std::mutex> lock(regionsMutex);
	ChunkRegion* region;
	auto it = regions.find(regionPos);
	if (it != regions.end())
	{
		// Region already exists, use it
		region = it->second;
	}
	else
	{
		// Region doesn't exist, create it
		// Aquire region from pool
		region = regionPool.acquire();

		// Add region to map
		regions.emplace(regionPos, region);
	}

	// Get chunk index in region
	size_t index = getChunkIndexInRegion(chunk->getPosition());

	// Add chunk to region
	ASSERT(region->chunks[index] == nullptr);
	region->chunks[index] = chunk;

	// Increase chunk count in region
	region->chunkCount++;
	ASSERT(region->chunkCount <= CHUNK_REGION_VOLUME);
}

void ChunkRegionManager::removeChunk(Chunk* chunk)
{
	// Get region position
	glm::ivec3 regionPos = getRegionPosition(chunk->getPosition());

	// Check if region exists
	//std::lock_guard<std::mutex> lock(regionsMutex);
	auto it = regions.find(regionPos);
	if (it == regions.end())
	{
		return;
	}

	// Region exists
	ChunkRegion* region = it->second;

	// Get chunk index in region
	size_t index = getChunkIndexInRegion(chunk->getPosition());

	// Remove chunk from region
	ASSERT(region->chunks[index] == chunk);
	region->chunks[index] = nullptr;

	// Decrease chunk count in region
	region->chunkCount--;
	ASSERT(region->chunkCount <= CHUNK_REGION_VOLUME);

	// If region is empty, remove it from map and return to the pool
	if (region->chunkCount == 0)
	{
		regions.erase(it);
		regionPool.release(region);
	}
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
