#pragma once
#include "ChunkRegion.h"

#include "Core/MemoryAllocation/FixedArenaObjectPool.h"
#include "Core/Hashes/ivec3Hasher.h"

#include "robin_hood.h"

class ChunkRegionManager
{
public:
	using ChunkRegionMap = robin_hood::unordered_flat_map<glm::ivec3, ChunkRegion*, ivec3Hasher>;
private:
	FixedArenaObjectPool<ChunkRegion, 16> chunkRegionPool;
	ChunkRegionMap chunkRegions;
public:
	void preparation(size_t regionCount);

	ChunkRegion* getRegion(const glm::ivec3& regionPosition);
	ChunkRegion* getOrCreateRegion(const glm::ivec3& regionPosition);
	bool doesRegionExist(const glm::ivec3& regionPosition) { return chunkRegions.contains(regionPosition); }
	void destroyChunkRegion(const glm::ivec3& regionPosition);

	const ChunkRegionMap& getRegionMap() const { return chunkRegions; }
	size_t getRegionCount() const { return chunkRegions.size(); }
};

