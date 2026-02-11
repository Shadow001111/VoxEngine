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
	static constexpr size_t MIN_FILE_SIZE = 2 + 2 + 1 + 1 + 1 + 1 + 2 + 2;
	static constexpr size_t MAX_FILE_SIZE = 2 + (MAX_PACKS * (2 + 1 + 64)) + CHUNK_VOLUME * (1 + 64 + 2 + 2);
public:
	using BlockChanges = robin_hood::unordered_flat_map<BlockId, std::vector<uint16_t>>;

	static std::filesystem::path CHUNK_SAVES_PATH;

	static bool filterChanges(BlockId BlockId, const std::vector<uint16_t>& indices, const BlockData*& outBlockData);

	static void loadBlocks(BlockChanges& blockChanges, const glm::ivec3 chunkPosition, BlockId* blocks);
	static void saveBlocks(BlockChanges& blockChanges, const glm::ivec3 chunkPosition, const BlockId* blocks);
};

