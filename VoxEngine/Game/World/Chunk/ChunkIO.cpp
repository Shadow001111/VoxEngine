#include "ChunkIO.h"

#include "Core/Profiler.h"
#include "Core/MemoryFileReader.h"
#include "Core/MemoryFileWriter.h"

#include "Game/DataPackManagment/AssetRegistry.h"

#include <map>
#include <format>

namespace fs = std::filesystem;

fs::path ChunkIO::CHUNK_SAVES_PATH;

bool ChunkIO::filterChanges(BlockId BlockId, const std::vector<uint16_t>& indices, const BlockData*& outBlockData)
{
	// Check indices range
	if (indices.size() == 0 || indices.size() > CHUNK_VOLUME)
	{
		return true;
	}
	// Get block data
	outBlockData = AssetRegistry::getBlockData(BlockId);
	if (!outBlockData)
	{
		return true;
	}
	// Check name length
	const auto& name = outBlockData->stringId;
	if (name.size() < 3 || name.size() > 64)
	{
		return true;
	}
	// Check for ':' symbol
	size_t colonPos = name.find(':');
	if (colonPos == std::string::npos)
	{
		return true;
	}
	size_t leftSize = colonPos;
	size_t rightSize = name.size() - 1 - colonPos;
	if (leftSize < 1 || leftSize > 64 || rightSize < 1 || rightSize > 64)
	{
		return true;
	}
	return false;
}

void ChunkIO::loadBlocks(BlockChanges& blockChanges, const glm::ivec3 chunkPosition, BlockId* blocks)
{
	PROFILE_SCOPE("Load chunk blocks", ProfileCategory::ChunkBlocks);

	std::string name = std::format("{}_{}_{}.bin", chunkPosition.x, chunkPosition.y, chunkPosition.z);
	fs::path filepath = CHUNK_SAVES_PATH / name;

	if (!fs::exists(filepath) || !fs::is_regular_file(filepath))
	{
		return;
	}

	MemoryFileReader file;
	auto loadResult = file.loadFile(filepath, MAX_FILE_SIZE);
	if (loadResult != MemoryFileReader::Result::Success)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Failed to open file.\n";
		return;
	}

	// Check file size
	if (file.getSize() < MIN_FILE_SIZE)
	{
		std::cerr << "[ChunkIO][loadBlocks]: File too small.\n";
		return;
	}

	// Get air block ID
	const BlockId AIR_BLOCK_ID = AssetRegistry::getBlockNumericalId("core:air");

	// Read pack count
	uint16_t packCount = 0;
	auto readResult = file.read(&packCount);
	if (readResult != MemoryFileReader::Result::Success)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error.\n";
		return;
	}
	else if (packCount == 0 || packCount > MAX_PACKS)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Pack count is invalid.\n";
		return;
	}

	// Clear existing changes
	blockChanges.clear();

	// Read all packs, blocks, and their indices
	for (uint16_t packIndex = 0; packIndex < packCount; packIndex++)
	{
		// Read block count for this pack
		uint16_t packBlockCount = 0;
		readResult = file.read(&packBlockCount);
		if (readResult != MemoryFileReader::Result::Success)
		{
			std::cerr << "[ChunkIO][loadBlocks]: Read error.\n";
			blockChanges.clear();
			return;
		}
		else if (packBlockCount == 0 || packBlockCount > CHUNK_VOLUME)
		{
			std::cerr << "[ChunkIO][loadBlocks]: Block count in pack is invalid.\n";
			blockChanges.clear();
			return;
		}

		// Read pack name
		uint8_t packNameLen = 0;
		readResult = file.read(&packNameLen);
		if (readResult != MemoryFileReader::Result::Success)
		{
			std::cerr << "[ChunkIO][loadBlocks]: Read error.\n";
			blockChanges.clear();
			return;
		}
		else if (packNameLen < 1 || packNameLen > 64)
		{
			std::cerr << "[ChunkIO][loadBlocks]: Pack name length is invalid.\n";
			blockChanges.clear();
			return;
		}

		std::string packName(packNameLen, '\0');
		readResult = file.read(&packName[0], packNameLen);
		if (readResult != MemoryFileReader::Result::Success)
		{
			std::cerr << "[ChunkIO][loadBlocks]: Read error.\n";
			blockChanges.clear();
			return;
		}

		// Read all blocks in this pack (with their indices immediately following)
		for (uint16_t blockIndex = 0; blockIndex < packBlockCount; blockIndex++)
		{
			// Read block name length
			uint8_t blockNameLen = 0;
			readResult = file.read(&blockNameLen);
			if (readResult != MemoryFileReader::Result::Success)
			{
				std::cerr << "[ChunkIO][loadBlocks]: Read error.\n";
				blockChanges.clear();
				return;
			}
			else if (blockNameLen < 1 || blockNameLen > 64)
			{
				std::cerr << "[ChunkIO][loadBlocks]: Block name length is invalid.\n";
				blockChanges.clear();
				return;
			}

			// Read block name
			std::string blockName(blockNameLen, '\0');
			readResult = file.read(&blockName[0], blockNameLen);
			if (readResult != MemoryFileReader::Result::Success)
			{
				std::cerr << "[ChunkIO][loadBlocks]: Read error.\n";
				blockChanges.clear();
				return;
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

			// Read indices count for this block
			uint16_t indicesCount = 0;
			readResult = file.read(&indicesCount);
			if (readResult != MemoryFileReader::Result::Success)
			{
				std::cerr << "[ChunkIO][loadBlocks]: Read error.\n";
				blockChanges.clear();
				return;
			}
			else if (indicesCount == 0 || indicesCount > CHUNK_VOLUME)
			{
				std::cerr << "[ChunkIO][loadBlocks]: Indices count is invalid.\n";
				blockChanges.clear();
				return;
			}

			// Read indices (empty count is allowed for removed blocks)
			std::vector<uint16_t> indices;
			indices.resize(indicesCount);
			readResult = file.read(indices.data(), indicesCount);
			if (readResult != MemoryFileReader::Result::Success)
			{
				std::cerr << "[ChunkIO][loadBlocks]: Read error.\n";
				blockChanges.clear();
				return;
			}

			// Store in changedBlocks map
			blockChanges[globalID] = std::move(indices);
		}
	}

	if (blockChanges.empty())
	{
		std::cerr << "[ChunkIO][loadBlocks]: No blocks loaded.\n";
		return;
	}

	// Verify we read the entire file
	if (!file.isEndOfFile())
	{
		std::cerr << "[ChunkIO][loadBlocks]: File size mismatch. Possible corruption.\n";
		blockChanges.clear();
		return;
	}

	// Apply loaded data to blocks array
	for (auto it = blockChanges.begin(); it != blockChanges.end();)
	{
		BlockId block = it->first;
		auto& indices = it->second;

		size_t writeIndex = 0;
		for (uint16_t index : indices)
		{
			if (index >= CHUNK_VOLUME || blocks[index] == block)
			{
				continue;
			}

			// Apply the change
			blocks[index] = block;
			indices[writeIndex++] = index;
		}

		if (writeIndex == 0)
		{
			// No valid indices left, erase this block type entirely
			it = blockChanges.erase(it);
		}
		else
		{
			indices.resize(writeIndex);
			++it;
		}
	}
}

void ChunkIO::saveBlocks(BlockChanges& blockChanges, const glm::ivec3 chunkPosition, const BlockId* blocks)
{
	if (blockChanges.empty())
	{
		return;
	}
	else if (blockChanges.size() > CHUNK_VOLUME)
	{
		std::cerr << "[ChunkIO][saveBlocks]: ChangedBlocks map size is invalid.\n";
		return;
	}

	PROFILE_SCOPE("Save chunk blocks", ProfileCategory::ChunkBlocks);

	std::string name = std::format("{}_{}_{}.bin", chunkPosition.x, chunkPosition.y, chunkPosition.z);
	fs::path filepath = CHUNK_SAVES_PATH / name;

	MemoryFileWriter file;
	auto initResult = file.initialize(MAX_FILE_SIZE);
	if (initResult != MemoryFileWriter::Result::Success)
	{
		std::cerr << "[ChunkIO][saveBlocks]: MemoryFileWriter failed to initialize.\n";
		return;
	}

	// Filter valid changes and collect block names
	std::map<BlockId, std::string> idToString;
	{
		const BlockData* blockData = nullptr;
		auto it = blockChanges.begin();
		while (it != blockChanges.end())
		{
			if (filterChanges(it->first, it->second, blockData))
			{
				it = blockChanges.erase(it);
			}
			else
			{
				idToString[it->first] = blockData->stringId;
				++it;
			}
		}
	}
	ASSERT(idToString.size() == blockChanges.size());
	if (idToString.empty())
	{
		return;
	}

	// Extract pack information from block names
	struct PackInfo
	{
		std::string name;
		std::vector<std::pair<BlockId, std::string>> blocks; // globalID -> blockName
	};

	robin_hood::unordered_flat_map<std::string, PackInfo> packMap;
	packMap.reserve(idToString.size());
	for (const auto& [globalID, fullName] : idToString)
	{
		size_t colonPos = fullName.find(':'); // Should be found, since we filtered out invalid cases

		std::string packName = fullName.substr(0, colonPos);
		std::string blockName = fullName.substr(colonPos + 1);

		auto& pack = packMap[packName];
		pack.name = packName;
		pack.blocks.emplace_back(globalID, blockName);
	}

	// Create sorted list of packs
	std::vector<PackInfo> packs;
	packs.reserve(packMap.size());
	for (auto& [name, pack] : packMap)
	{
		// Sort blocks within each pack for consistent ordering
		std::sort(pack.blocks.begin(), pack.blocks.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });
		packs.push_back(std::move(pack));
	}

	// Sort packs by name for consistent ordering
	std::sort(packs.begin(), packs.end(),
		[](const auto& a, const auto& b) { return a.name < b.name; });

	// Write pack count
	uint16_t packCount = static_cast<uint16_t>(packs.size());
	file.write(&packCount);

	// Write each pack and its blocks with indices immediately after each block
	for (const auto& pack : packs)
	{
		// Write block count for this pack
		uint16_t blockCount = static_cast<uint16_t>(pack.blocks.size());
		file.write(&blockCount);

		// Write pack name
		uint8_t packNameLen = static_cast<uint8_t>(pack.name.size());
		file.write(&packNameLen);
		file.writeBytes(pack.name.data(), packNameLen);

		// Write each block in pack
		for (const auto& [globalID, blockName] : pack.blocks)
		{
			// Write block name
			uint8_t blockNameLen = static_cast<uint8_t>(blockName.size());
			file.write(&blockNameLen);
			file.writeBytes(blockName.data(), blockNameLen);

			// Write indices for this block
			const auto& indices = blockChanges.at(globalID);
			uint16_t indicesCount = static_cast<uint16_t>(indices.size());
			file.write(&indicesCount);
			if (indicesCount > 0)
			{
				file.write(indices.data(), indicesCount);
			}
		}
	}

	// Save file
	auto saveResult = file.saveToFile(filepath);
	if (saveResult != MemoryFileWriter::Result::Success)
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to save file: " << static_cast<int>(saveResult) << "\n";
		return;
	}
}