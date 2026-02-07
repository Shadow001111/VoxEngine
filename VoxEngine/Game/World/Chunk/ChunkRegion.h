#pragma once
#include "robin_hood.h"
#include "Metrics.h"
#include "Core/MemoryAllocation/FixedArenaObjectPool.h"
#include "Core/Hashes/ivec3Hasher.h"

#include <glm/glm.hpp>
#include <array>
#include <mutex>

class Chunk;

class ChunkRegion
{
	friend class ChunkRegionManager;

	std::array<Chunk*, CHUNK_REGION_VOLUME> chunks{};
public:
	ChunkRegion() = default;
	~ChunkRegion() = default;
	ChunkRegion(const ChunkRegion&) = delete;
	ChunkRegion& operator=(const ChunkRegion&) = delete;
	ChunkRegion(ChunkRegion&&) = delete;
	ChunkRegion& operator=(ChunkRegion&&) = delete;

	const auto& getChunks() const { return chunks; };
	const size_t getChunkCount() const;
};

class ChunkRegionManager
{
	FixedArenaObjectPool<ChunkRegion, 4> regionPool;

	robin_hood::unordered_flat_map<glm::ivec3, ChunkRegion*, ivec3Hasher> regions;
	//mutable std::mutex regionsMutex;

	glm::ivec3 getRegionPosition(const glm::ivec3& chunkPosition) const;
	size_t getChunkIndexInRegion(const glm::ivec3& chunkPosition) const;
public:
	ChunkRegionManager() = default;
	~ChunkRegionManager() = default;
	ChunkRegionManager(const ChunkRegionManager&) = delete;
	ChunkRegionManager& operator=(const ChunkRegionManager&) = delete;
	ChunkRegionManager(ChunkRegionManager&&) = delete;
	ChunkRegionManager& operator=(ChunkRegionManager&&) = delete;

	void addChunk(Chunk* chunk);
	void removeChunk(Chunk* chunk);

	size_t getRegionCount() const;
	size_t getChunkCount() const;

	const auto& getRegions() const { return regions; }
	//const auto& getRegionMutex() const { return regionsMutex; }
};
