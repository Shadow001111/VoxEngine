#include "ChunkRegionManager.h"

void ChunkRegionManager::preparation(size_t regionCount)
{
	chunkRegions.reserve(regionCount);
	chunkRegionPool.allocate(regionCount);
}

ChunkRegion* ChunkRegionManager::getRegion(const glm::ivec3& regionPosition)
{
	auto it = chunkRegions.find(regionPosition);
	return it != chunkRegions.end() ? it->second : nullptr;
}

ChunkRegion* ChunkRegionManager::getOrCreateRegion(const glm::ivec3& regionPosition)
{
	// Check if region already exists
	auto it = chunkRegions.find(regionPosition);
	if (it != chunkRegions.end())
	{
		// Region already exists, use it
		return it->second;
	}

	// Region doesn't exist, create it
	// Aquire region from pool
	ChunkRegion* region = chunkRegionPool.acquire();
	
	// Initialize region
	region->init();

	// Add region to map
	chunkRegions.emplace(regionPosition, region);

	return region;
}

void ChunkRegionManager::destroyChunkRegion(const glm::ivec3& regionPosition)
{
	// Check if region already exists
	auto it = chunkRegions.find(regionPosition);
	if (it == chunkRegions.end())
	{
		return;
	}

	// Regions exists
	ChunkRegion* region = it->second;
	chunkRegions.erase(it);
	chunkRegionPool.release(region);
}
