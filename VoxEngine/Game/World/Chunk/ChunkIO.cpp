#include "ChunkIO.h"

#include "Game/DataPackManagment/AssetRegistry.h"

#include "Core/FileStream.h"
#include "Game/TracyProfiler.h"

#include <format>
#include <iostream>

namespace fs = std::filesystem;


bool doesFileExist(const std::filesystem::path& path)
{
	std::error_code ec;
	auto status = fs::status(path, ec);
	return !ec && fs::is_regular_file(status);
}

bool doesDirectoryExist(const std::filesystem::path& path)
{
	std::error_code ec;
	auto status = fs::status(path, ec);
	return !ec && fs::is_directory(status);
}


fs::path ChunkIO::CHUNK_SAVES_PATH;


fs::path ChunkIO::getFilePathFromPosition(const glm::ivec3& position)
{
	std::string name = std::format("{}_{}_{}.bin", position.x, position.y, position.z);
	return CHUNK_SAVES_PATH / name;
}

uint64_t ChunkIO::computeHash(const BlockChanges& blockChanges)
{
	// Start with a seed
	uint64_t hash = 0xCBF29CE484222325ULL; // FNV-1a offset basis

	// Sort block IDs for consistent ordering
	std::vector<BlockId> blockIds;
	blockIds.reserve(blockChanges.size());
	for (const auto& [blockId, _] : blockChanges)
	{
		blockIds.push_back(blockId);
	}
	std::sort(blockIds.begin(), blockIds.end());

	// Hash each block ID and its indices
	for (BlockId blockId : blockIds)
	{
		const auto& indices = blockChanges.at(blockId);

		// Hash block ID
		hash ^= static_cast<uint64_t>(blockId);
		hash *= 0x100000001B3ULL;

		// Sort indices for consistent ordering
		std::vector<uint16_t> sortedIndices = indices;
		std::sort(sortedIndices.begin(), sortedIndices.end());

		// Hash each index
		for (uint16_t index : sortedIndices)
		{
			hash ^= static_cast<uint64_t>(index);
			hash *= 0x100000001B3ULL;
		}
	}

	return hash;
}

bool ChunkIO::areChangesValid(BlockId BlockId, const std::vector<uint16_t>& indices, const BlockData*& outBlockData)
{
	// Check indices range
	if (indices.size() == 0 || indices.size() > CHUNK_VOLUME)
	{
		return false;
	}

	// Get block data
	outBlockData = AssetRegistry::getBlockData(BlockId);
	if (!outBlockData)
	{
		return false;
	}

	// Check stringId length
	const auto& stringId = outBlockData->stringId;
	if (stringId.size() < 3 || stringId.size() > MAX_OBJECT_NAME_SIZE * 2 + 1)
	{
		return false;
	}

	// Check for ':' symbol
	size_t colonPos = stringId.find(':');
	if (colonPos == std::string::npos)
	{
		return false;
	}

	// Check lengths of left and right parts
	size_t leftSize = colonPos;
	size_t rightSize = stringId.size() - 1 - colonPos;
	if (leftSize < 1 || leftSize > MAX_OBJECT_NAME_SIZE || rightSize < 1 || rightSize > MAX_OBJECT_NAME_SIZE)
	{
		return false;
	}
	return true;
}

bool ChunkIO::readPackCount(FileStream& reader, uint16_t& packCount)
{
	if (!reader.read(&packCount))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error for pack count.\n";
		return false;
	}

	if (packCount == 0 || packCount > MAX_PACKS)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Pack count is invalid.\n";
		return false;
	}

	return true;
}

bool ChunkIO::readPackBlockCount(FileStream& reader, uint16_t& packBlockCount)
{
	if (!reader.read(&packBlockCount))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error for pack block count.\n";
		return false;
	}

	if (packBlockCount == 0 || packBlockCount > CHUNK_VOLUME)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Block count in pack is invalid.\n";
		return false;
	}

	return true;
}

bool ChunkIO::readPackName(FileStream& reader, std::string& packName)
{
	uint8_t packNameLen = 0;
	if (!reader.read(&packNameLen))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error for pack name length.\n";
		return false;
	}

	if (packNameLen < 1 || packNameLen > 64)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Pack name length is invalid.\n";
		return false;
	}

	std::string packName_(packNameLen, '\0');
	if (!reader.readBytes(&packName_[0], packNameLen))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error for pack name.\n";
		return false;
	}

	packName = std::move(packName_);
	return true;
}

bool ChunkIO::readBlockName(FileStream& reader, std::string& blockName)
{
	// Read block name length
	uint8_t blockNameLen = 0;
	if (!reader.read(&blockNameLen))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error for block name length.\n";
		return false;
	}

	if (blockNameLen < 1 || blockNameLen > 64)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Block name length is invalid.\n";
		return false;
	}

	// Read block name
	std::string blockName_(blockNameLen, '\0');
	if (!reader.readBytes(&blockName_[0], blockNameLen))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error for block name.\n";
		return false;
	}

	blockName = std::move(blockName_);
	return true;
}

bool ChunkIO::readIndices(FileStream& reader, std::vector<uint16_t>& indices)
{
	// Read indices count for this block
	uint16_t indicesCount = 0;
	if (!reader.read(&indicesCount))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error for indices count.\n";
		return false;
	}

	if (indicesCount == 0 || indicesCount > CHUNK_VOLUME)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Indices count is invalid.\n";
		return false;
	}

	// Read indices
	indices.resize(indicesCount);
	if (!reader.read(indices.data(), indicesCount))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error for indices.\n";
		return false;
	}

	return true;
}

bool ChunkIO::loadBlockChanges(const std::filesystem::path& filepath, BlockChanges& blockChanges)
{
	// Load file
	FileStream file(filepath, FileStream::Mode::Read);
	if (!file)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Failed to open file.\n";
		return false;
	}

	// Get air block ID
	const BlockId AIR_BLOCK_ID = AssetRegistry::getBlockNumericalId("core:air");

	// Skip hash value
	if (!file.skip(8))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Failed to skip hash.\n";
		return false;
	}

	// Read pack count
	uint16_t packCount = 0;
	readPackCount(file, packCount);

	// Clear existing changes
	blockChanges.clear();

	// Read all packs, blocks, and their indices
	for (uint16_t packIndex = 0; packIndex < packCount; packIndex++)
	{
		// Read block count for this pack
		uint16_t packBlockCount = 0;
		if (!readPackBlockCount(file, packBlockCount))
		{
			return false;
		}

		// Read pack name
		std::string packName;
		if (!readPackName(file, packName))
		{
			return false;
		}

		// Read all blocks in this pack (with their indices immediately following)
		for (uint16_t blockIndex = 0; blockIndex < packBlockCount; blockIndex++)
		{
			// Read block name
			std::string blockName;
			if (!readBlockName(file, blockName))
			{
				return false;
			}

			// Construct full block name
			std::string fullName = std::format("{}:{}", packName, blockName);

			// Convert to global BlockId
			BlockId globalID = AssetRegistry::getBlockNumericalId(fullName);
			if (globalID == (BlockId)-1)
			{
				// Block no longer exists - fallback to air
				globalID = AIR_BLOCK_ID;
				std::cerr << "[ChunkIO][loadBlocks]: Block '" << fullName << "' is not found. Replaced with air.\n";
			}

			// Read indices
			std::vector<uint16_t> indices;
			if (!readIndices(file, indices))
			{
				return false;
			}

			// Store in changedBlocks map
			blockChanges[globalID] = std::move(indices);
		}
	}

	if (blockChanges.empty())
	{
		std::cerr << "[ChunkIO][loadBlocks]: No blocks loaded.\n";
		return false;
	}

	return true;
}

bool ChunkIO::checkIfShouldBeSaved(FileStream& file, uint64_t hashValue)
{
	// Check if file exists and is open
	if (!file)
	{
		return true;
	}
	
	// Read hash value
	uint64_t storedHasValue;
	if (!file.read(&storedHasValue))
	{
		std::cerr << "[ChunkIO][saveBlocks]: Read error for hash value.\n";
		return true;
	}

	// Compare hash value
	return hashValue != storedHasValue; // Write to file only if hash values are different
}

ChunkIO::Map<BlockId, std::string> ChunkIO::collectBlockIdStrings(const BlockChanges& blockChanges)
{
	robin_hood::unordered_flat_map<BlockId, std::string> idToString;

	const BlockData* blockData = nullptr;
	for (const auto& [blockId, indices] : blockChanges)
	{
		if (areChangesValid(blockId, indices, blockData))
		{
			idToString.emplace(blockId, blockData->stringId);
		}
	}

	return idToString;
}

ChunkIO::Map<std::string, ChunkIO::PackInfo> ChunkIO::transformBlockDataIntoPackData(const Map<BlockId, std::string>& blockIdToString)
{
	robin_hood::unordered_flat_map<std::string, PackInfo> packMap;
	packMap.reserve(blockIdToString.size());
	for (const auto& [globalID, fullName] : blockIdToString)
	{
		size_t colonPos = fullName.find(':'); // Should be found, since we filtered out invalid cases

		std::string packName = fullName.substr(0, colonPos);
		std::string blockName = fullName.substr(colonPos + 1);

		auto& pack = packMap[packName]; // Fine pack by packName
		pack.name = packName;
		pack.blocks.emplace_back(globalID, blockName);
	}
	return packMap;
}

std::vector<ChunkIO::PackInfo> ChunkIO::transformPackDataMapToSortedVector(const Map<std::string, PackInfo>& packDataMap)
{
	// Create vector of packs
	std::vector<PackInfo> packs;
	packs.reserve(packDataMap.size());
	for (auto& [name, pack] : packDataMap)
	{
		auto& packFromVector = packs.emplace_back(pack);

		// Sort blocks within each pack for consistent ordering
		std::sort(packFromVector.blocks.begin(), packFromVector.blocks.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });
	}

	// Sort packs by name for consistent ordering
	std::sort(packs.begin(), packs.end(),
		[](const auto& a, const auto& b) { return a.name < b.name; });

	return packs;
}

void ChunkIO::loadBlocks(BlockChanges& blockChanges, const glm::ivec3& chunkRegionPosition, size_t chunkIndexInRegion)
{
	// Get chunk file path
	fs::path chunkRegionDirectoryPath = getFilePathFromPosition(chunkRegionPosition);
	fs::path chunkFilePath = chunkRegionDirectoryPath / std::format("{}.bin", chunkIndexInRegion);

	// No need to check if file exists - loadBlockChanges will handle that and return false if it doesn't exist or fails to load for any reason

	// Load and apply changes
	bool success = loadBlockChanges(chunkFilePath, blockChanges);
	if (!success)
	{
		blockChanges.clear();
	}
}

void ChunkIO::saveBlocks(const BlockChanges& blockChanges, const glm::ivec3& chunkRegionPosition, size_t chunkIndexInRegion)
{
	TRACY_SCOPE_NC("Save blocks", ProfileCategory::ChunkIO);

	if (blockChanges.empty())
	{
		return;
	}
	else if (blockChanges.size() > CHUNK_VOLUME)
	{
		std::cerr << "[ChunkIO][saveBlocks]: ChangedBlocks map size is invalid.\n";
		return;
	}

	// Compute hash value
	uint64_t hashValue = computeHash(blockChanges);

	// Get chunk file path
	fs::path chunkRegionDirectoryPath = getFilePathFromPosition(chunkRegionPosition);
	fs::path chunkFilePath = chunkRegionDirectoryPath / std::format("{}.bin", chunkIndexInRegion);

	// Ensure region directory exists
	std::error_code ec;
	fs::create_directories(chunkRegionDirectoryPath, ec);
	if (ec)
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to create directory '"
			<< chunkRegionDirectoryPath << "'. Error: " << ec.message() << "\n";
		return;
	}

	// Open file for reading
	FileStream file(chunkFilePath, FileStream::Mode::Read);

	// Check if we actually need to write to file (if changes are the same as what's already on drive, skip writing)
	if (!checkIfShouldBeSaved(file, hashValue))
	{
		return;
	}

	// Open file for writing
	file.close();
	file.open(chunkFilePath, FileStream::Mode::Write);
	if (!file)
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to create file.\n";
		return;
	}

	// Filter valid changes and collect block names
	Map<BlockId, std::string> idToString = collectBlockIdStrings(blockChanges);
	
	// Check if any block id strings were collected
	if (idToString.empty())
	{
		return; // All changes were discarded
	}

	// Extract pack information from block names
	Map<std::string, PackInfo> packMap = transformBlockDataIntoPackData(idToString);

	// Create sorted list of packs
	std::vector<PackInfo> packs = transformPackDataMapToSortedVector(packMap);

	// Write hash value
	if (!file.write(&hashValue))
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to write has value.\n";
		return;
	}

	// Write pack count
	uint16_t packCount = static_cast<uint16_t>(packs.size());
	if (!file.write(&packCount))
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to write pack count.\n";
		return;
	}

	// Write each pack and its blocks with indices immediately after each block
	for (const auto& pack : packs)
	{
		// Write block count for this pack
		uint16_t blockCount = static_cast<uint16_t>(pack.blocks.size());
		if (!file.write(&blockCount))
		{
			std::cerr << "[ChunkIO][saveBlocks]: Failed to write block count.\n";
			return;
		}

		// Write pack name
		uint8_t packNameLen = static_cast<uint8_t>(pack.name.size());
		if (!file.write(&packNameLen) ||
			!file.writeBytes(pack.name.data(), packNameLen))
		{
			std::cerr << "[ChunkIO][saveBlocks]: Failed to write pack name.\n";
			return;
		}

		// Write each block in pack
		for (const auto& [globalID, blockName] : pack.blocks)
		{
			// Write block name
			uint8_t blockNameLen = static_cast<uint8_t>(blockName.size());
			if (!file.write(&blockNameLen) ||
				!file.writeBytes(blockName.data(), blockNameLen))
			{
				std::cerr << "[ChunkIO][saveBlocks]: Failed to write block name.\n";
				return;
			}

			// Write indices for this block
			const auto& indices = blockChanges.at(globalID);
			uint16_t indicesCount = static_cast<uint16_t>(indices.size());
			if (!file.write(&indicesCount) ||
				(indicesCount > 0 && !file.write(indices.data(), indicesCount)))
			{
				std::cerr << "[ChunkIO][saveBlocks]: Failed to write indices.\n";
				return;
			}
		}
	}

	// Ensure everything is written to disk
	if (!file.flush())
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to flush data to disk.\n";
	}
}

AtomicBitset<CHUNK_REGION_VOLUME, size_t> ChunkIO::checkChunkRegionForSaves(const glm::ivec3& regionPosition)
{
	TRACY_SCOPE_NC("Scan chunk region directory", ProfileCategory::ChunkIO);

	AtomicBitset<CHUNK_REGION_VOLUME, size_t> mask;

	// Get region file path
	fs::path chunkRegionDirectoryPath = getFilePathFromPosition(regionPosition);

	// Check if directory exists
	if (!doesDirectoryExist(chunkRegionDirectoryPath))
	{
		return mask;
	}

	// Iterate through all chunk files in the region directory
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(chunkRegionDirectoryPath, ec))
	{
		// Check if file is a regular file
		if (!entry.is_regular_file(ec))
		{
			continue;
		}

		// Get chunk index from file name (string to size_t)
		std::string chunkFileName = entry.path().stem().string();
		size_t chunkIndex;
		try
		{
			chunkIndex = std::stoull(chunkFileName);
		}
		catch (const std::exception&)
		{
			std::cerr << "[ChunkIO][checkChunkRegionForBlockChanges]: Invalid chunk file name '" << chunkFileName << "'. Skipping.\n";
			continue;
		}

		// Check if chunk index is within valid range
		if (chunkIndex >= CHUNK_REGION_VOLUME)
		{
			continue;
		}

		// Set bit in mask for this chunk index
		mask.set(chunkIndex, true);
	}

	return mask;
}
