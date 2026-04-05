#pragma once
#include "Game/DataPackManagment/DataTypes/BlockData.h"

#include "Core/AtomicBitset.h"

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

	template<typename T1, typename T2>
	using Map = robin_hood::unordered_flat_map<T1, T2>;
public:
	using BlockChanges = Map<BlockId, std::vector<uint16_t>>;
private:
	static constexpr size_t MAX_PACKS = CHUNK_VOLUME;

	static std::filesystem::path getFilePathFromPosition(const glm::ivec3& position);

	static uint64_t computeHash(const BlockChanges& blockChanges);

	static bool areChangesValid(BlockId BlockId, const std::vector<uint16_t>& indices, const BlockData*& outBlockData);

	// READ SECTION

	static bool readPackCount(StreamReader& reader, uint16_t& packCount);

	static bool readPackBlockCount(StreamReader& reader, uint16_t& packBlockCount);
	static bool readPackName(StreamReader& reader, std::string& packName);

	static bool readBlockName(StreamReader& reader, std::string& blockName);
	static bool readIndices(StreamReader& reader, std::vector<uint16_t>& indices);

	static bool loadBlockChanges(const std::filesystem::path& filepath, BlockChanges& blockChanges);
	static void applyBlockChanges(BlockChanges& blockChanges, BlockId* blocks);

	// WRITE SECTION

	static bool checkIfShouldBeSaved(const std::filesystem::path& filepath, uint64_t hashValue);

	static Map<BlockId, std::string> collectBlockIdStrings(const BlockChanges& blockChanges);

	static Map<std::string, PackInfo> transformBlockDataIntoPackData(const Map<BlockId, std::string>& blockIdToString);

	static std::vector<PackInfo> transformPackDataMapToSortedVector(const Map<std::string, PackInfo>& packDataMap);
public:
	using BlockChanges = Map<BlockId, std::vector<uint16_t>>;

	static std::filesystem::path CHUNK_SAVES_PATH;

	static void loadBlocks(BlockChanges& blockChanges, BlockId* blocks, const glm::ivec3& chunkRegionPosition, size_t chunkIndexInRegion);
	static void saveBlocks(const BlockChanges& blockChanges, const glm::ivec3& chunkRegionPosition, size_t chunkIndexInRegion);

	static AtomicBitset<CHUNK_REGION_VOLUME, size_t> checkChunkRegionForSaves(const glm::ivec3& regionPosition);
};

