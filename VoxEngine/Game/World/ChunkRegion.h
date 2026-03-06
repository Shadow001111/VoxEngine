#pragma once
#include "Chunk/Metrics.h"

#include "Core/AtomicFlags.h"

#include <glm/glm.hpp>
#include <array>

class Chunk;
class WorldChunkManager;

class ChunkRegion
{
	friend class WorldChunkManager;

	std::array<Chunk*, CHUNK_REGION_VOLUME> chunks{};
	uint32_t chunkCount = 0; // Number of chunks currently in region. Used to determine if region is empty and can be removed.

	AtomicFlags<uint8_t> flags;
public:
	enum class Flag : uint8_t
	{
		HasMeshToUpload = 0
	};

	ChunkRegion() = default;
	~ChunkRegion() = default;
	ChunkRegion(const ChunkRegion&) = delete;
	ChunkRegion& operator=(const ChunkRegion&) = delete;
	ChunkRegion(ChunkRegion&&) = delete;
	ChunkRegion& operator=(ChunkRegion&&) = delete;

	void init();

	void setFlag(Flag flag, bool value) { flags.set(static_cast<unsigned>(flag), value); }
	[[nodiscard]] bool readFlag(Flag flag) const { return flags.read(static_cast<unsigned>(flag)); }
	[[nodiscard]] bool readAndSetFlag(Flag flag, bool value) { return flags.readAndSet(static_cast<unsigned>(flag), value); }

	[[nodiscard]] const auto& getChunks() const { return chunks; };
	[[nodiscard]] const size_t getChunkCount() const { return chunkCount; };

	[[nodiscard]] static glm::ivec3 getRegionPosition(const glm::ivec3& chunkPosition);
	[[nodiscard]] static size_t getChunkIndexInRegion(const glm::ivec3& chunkPosition);
};
