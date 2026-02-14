#pragma once
#include "Game/DataPackManagment/DataTypes/BlockData.h"

#include "Metrics.h"

#include "robin_hood.h"
#include <vector>
#include <cstdint>
#include <filesystem>
#include <glm/vec3.hpp>

class ChunkIO
{
	static constexpr size_t MAX_PACKS = CHUNK_VOLUME;

	static constexpr size_t MAX_ALLOWED_FILE_SIZE = (size_t)2 << 20; // 2 mb
public:
	using BlockChanges = robin_hood::unordered_flat_map<BlockId, std::vector<uint16_t>>;

	static std::filesystem::path CHUNK_SAVES_PATH;

	static uint64_t computeHash(const BlockChanges& blockChanges);

	static bool filterChanges(BlockId BlockId, const std::vector<uint16_t>& indices, const BlockData*& outBlockData);

	static void loadBlocks(BlockChanges& blockChanges, const glm::ivec3 chunkPosition, BlockId* blocks);
	static void saveBlocks(BlockChanges& blockChanges, const glm::ivec3 chunkPosition, const BlockId* blocks);
};

