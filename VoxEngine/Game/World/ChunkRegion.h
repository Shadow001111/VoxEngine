#pragma once
#include "Chunk/Metrics.h"

#include "Core/AtomicFlags.h"
#include "Core/AtomicBitset.h"

#include <glm/glm.hpp>
#include <array>

class Chunk;
class WorldChunkManager;

class ChunkRegion
{
	friend class WorldChunkManager;

	std::array<Chunk*, CHUNK_REGION_VOLUME> chunks{};
	uint8_t chunkCount = 0; // Number of chunks currently in region. Used to determine if region is empty and can be removed.
	std::atomic<uint8_t> renderChunkCount = 0;

	AtomicFlags<uint8_t> flags;
public:
	bool isFrustumCulled = true;
private:
	glm::ivec3 position;

	AtomicBitset<CHUNK_REGION_VOLUME, size_t> savedChunksMask;
	std::atomic<bool> isSavedChunksMaskInitialized{ false };

	static AtomicFlags<uint8_t> globalFlags;
public:
	enum class Flag : uint8_t
	{
		HasMeshToUpload = 0,
		HasMeshToUpdate,
		HasLightToUpdate,
		HasConnectivityToUpdate
	};

	ChunkRegion() = default;
	~ChunkRegion() = default;
	ChunkRegion(const ChunkRegion&) = delete;
	ChunkRegion& operator=(const ChunkRegion&) = delete;
	ChunkRegion(ChunkRegion&&) = delete;
	ChunkRegion& operator=(ChunkRegion&&) = delete;

	void init(const glm::ivec3& regionPosition);

	void setHasSavedData(size_t chunkIndex, bool value) noexcept
	{
		isSavedChunksMaskInitialized.wait(false, std::memory_order_acquire);
		savedChunksMask.set(chunkIndex, value);
	}
	[[nodiscard]] bool hasSavedData(size_t chunkIndex) const noexcept
	{
		isSavedChunksMaskInitialized.wait(false, std::memory_order_acquire);
		return savedChunksMask.read(chunkIndex);
	}

	void setFlag(Flag flag, bool value) noexcept { flags.set(static_cast<unsigned>(flag), value); }
	[[nodiscard]] bool readFlag(Flag flag) const noexcept { return flags.read(static_cast<unsigned>(flag)); }
	[[nodiscard]] bool readAndSetFlag(Flag flag, bool value) noexcept { return flags.readAndSet(static_cast<unsigned>(flag), value); }

	static void setGlobalFlag(Flag flag, bool value) noexcept { globalFlags.set(static_cast<unsigned>(flag), value); }
	[[nodiscard]] static bool readGlobalFlag(Flag flag) noexcept { return globalFlags.read(static_cast<unsigned>(flag)); }
	[[nodiscard]] static bool readAndSetGlobalFlag(Flag flag, bool value) noexcept { return globalFlags.readAndSet(static_cast<unsigned>(flag), value); }

	[[nodiscard]] const auto& getChunks() const noexcept { return chunks; };
	[[nodiscard]] const size_t getChunkCount() const noexcept { return chunkCount; };

	[[nodiscard]] static glm::ivec3 getRegionPosition(const glm::ivec3& chunkPosition);
	[[nodiscard]] static size_t getChunkIndexInRegion(const glm::ivec3& chunkPosition);

	void incrementRenderChunkCount() noexcept { renderChunkCount.fetch_add(1, std::memory_order_acq_rel); }
	void decrementRenderChunkCount() noexcept { renderChunkCount.fetch_sub(1, std::memory_order_acq_rel); }
	[[nodiscard]] size_t getRenderChunkCount() const noexcept { return renderChunkCount.load(std::memory_order_acquire); };
	[[nodiscard]] const bool hasRenderChunks() const noexcept { return renderChunkCount.load(std::memory_order_acquire) > 0; };

	[[nodiscard]] glm::ivec3 getPosition() const noexcept { return position; }
};
