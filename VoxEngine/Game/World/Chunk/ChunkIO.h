#pragma once
#include "Game/DataPackManagment/DataTypes/BlockData.h"

#include "Metrics.h"

#include "robin_hood.h"
#include <vector>
#include <cstdint>
#include <filesystem>
#include <glm/vec3.hpp>

class StreamReader;

class ChunkIO
{
	struct PackInfo
	{
		std::string name;
		std::vector<std::pair<BlockId, std::string>> blocks; // globalID -> blockName

		// TODO: Define copying and moving
	};
public:
	using BlockChanges = robin_hood::unordered_flat_map<BlockId, std::vector<uint16_t>>;
private:
	static constexpr size_t MAX_PACKS = CHUNK_VOLUME;

	static std::filesystem::path getFilePath(const glm::ivec3 chunkPosition);

	static bool doesFileExist(const std::filesystem::path& path);

	static uint64_t computeHash(const BlockChanges& blockChanges);

	static bool areChangesValid(BlockId BlockId, const std::vector<uint16_t>& indices, const BlockData*& outBlockData);

	// READ SECTION

	static bool readPackCount(StreamReader& reader, uint16_t& packCount);

	static bool readPackBlockCount(StreamReader& reader, uint16_t& packBlockCount);
	static bool readPackName(StreamReader& reader, std::string& packName);

	static bool readBlockName(StreamReader& reader, std::string& blockName);
	static bool readIndices(StreamReader& reader, std::vector<uint16_t>& indices);

	static void loadBlockChanges(const std::filesystem::path& filepath, BlockChanges& blockChanges);
	static void applyBlockChanges(BlockChanges& blockChanges, BlockId* blocks);

	// WRITE SECTION

	static bool checkIfShouldBeSaved(const std::filesystem::path& filepath, uint64_t hashValue);

	static robin_hood::unordered_flat_map<BlockId, std::string> collectBlockIdStrings(const BlockChanges& blockChanges);

	static robin_hood::unordered_flat_map<std::string, PackInfo> transformBlockDataIntoPackData(const robin_hood::unordered_flat_map<BlockId, std::string>& blockIdToString);

	static std::vector<PackInfo> transformPackDataMapToSortedVector(const robin_hood::unordered_flat_map<std::string, PackInfo>& packDataMap);
public:
	using BlockChanges = robin_hood::unordered_flat_map<BlockId, std::vector<uint16_t>>;

	static std::filesystem::path CHUNK_SAVES_PATH;

	static void loadBlocks(BlockChanges& blockChanges, const glm::ivec3& chunkPosition, BlockId* blocks);
	static void saveBlocks(const BlockChanges& blockChanges, const glm::ivec3& chunkPosition, const BlockId* blocks);
};

