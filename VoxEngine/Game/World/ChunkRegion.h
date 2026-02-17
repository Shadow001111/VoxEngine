#pragma once
#include "Chunk/Metrics.h"

#include <glm/glm.hpp>
#include <array>

class Chunk;
class WorldChunkManager;

class ChunkRegion
{
	friend class WorldChunkManager;

	std::array<Chunk*, CHUNK_REGION_VOLUME> chunks{};
	uint32_t chunkCount = 0; // Number of chunks currently in region. Used to determine if region is empty and can be removed.
public:
	ChunkRegion() = default;
	~ChunkRegion() = default;
	ChunkRegion(const ChunkRegion&) = delete;
	ChunkRegion& operator=(const ChunkRegion&) = delete;
	ChunkRegion(ChunkRegion&&) = delete;
	ChunkRegion& operator=(ChunkRegion&&) = delete;

	void init();

	const auto& getChunks() const { return chunks; };
	const size_t getChunkCount() const { return chunkCount; };

	static glm::ivec3 getRegionPosition(const glm::ivec3& chunkPosition);
	static size_t getChunkIndexInRegion(const glm::ivec3& chunkPosition);
};
