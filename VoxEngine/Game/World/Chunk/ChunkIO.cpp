#include "ChunkIO.h"

#include "Core/Profiler.h"
#include "Core/Stream/StreamReader.h"
#include "Core/Stream/StreamWriter.h"

#include "Game/DataPackManagment/AssetRegistry.h"

#include <map>
#include <format>

namespace fs = std::filesystem;

fs::path ChunkIO::CHUNK_SAVES_PATH;

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

	// Check if file exists
	if (!fs::exists(filepath) || !fs::is_regular_file(filepath))
	{
		return;
	}

	// Load file
	StreamReader reader(filepath);
	if (!reader)
	{
		std::cerr << "[ChunkIO][loadBlocks]: Failed to open file.\n";
		return;
	}

	// Get air block ID
	const BlockId AIR_BLOCK_ID = AssetRegistry::getBlockNumericalId("core:air");

	// Skip hash value
	if (!reader.skip(8))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Failed to skip hash.\n";
		return;
	}

	// Read pack count
	uint16_t packCount = 0;
	if (!reader.read(&packCount))
	{
		std::cerr << "[ChunkIO][loadBlocks]: Read error for pack count.\n";
		return;
	}
	
	if (packCount == 0 || packCount > MAX_PACKS)
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
		if (!reader.read(&packBlockCount))
		{
			std::cerr << "[ChunkIO][loadBlocks]: Read error for pack block count.\n";
			blockChanges.clear();
			return;
		}

		if (packBlockCount == 0 || packBlockCount > CHUNK_VOLUME)
		{
			std::cerr << "[ChunkIO][loadBlocks]: Block count in pack is invalid.\n";
			blockChanges.clear();
			return;
		}

		// Read pack name
		uint8_t packNameLen = 0;
		if (!reader.read(&packNameLen))
		{
			std::cerr << "[ChunkIO][loadBlocks]: Read error for pack name length.\n";
			blockChanges.clear();
			return;
		}
		
		if (packNameLen < 1 || packNameLen > 64)
		{
			std::cerr << "[ChunkIO][loadBlocks]: Pack name length is invalid.\n";
			blockChanges.clear();
			return;
		}

		std::string packName(packNameLen, '\0');
		if (!reader.readBytes(&packName[0], packNameLen))
		{
			std::cerr << "[ChunkIO][loadBlocks]: Read error for pack name.\n";
			blockChanges.clear();
			return;
		}

		// Read all blocks in this pack (with their indices immediately following)
		for (uint16_t blockIndex = 0; blockIndex < packBlockCount; blockIndex++)
		{
			// Read block name length
			uint8_t blockNameLen = 0;
			if (!reader.read(&blockNameLen))
			{
				std::cerr << "[ChunkIO][loadBlocks]: Read error for block name length.\n";
				blockChanges.clear();
				return;
			}
			
			if (blockNameLen < 1 || blockNameLen > 64)
			{
				std::cerr << "[ChunkIO][loadBlocks]: Block name length is invalid.\n";
				blockChanges.clear();
				return;
			}

			// Read block name
			std::string blockName(blockNameLen, '\0');
			if (!reader.readBytes(&blockName[0], blockNameLen))
			{
				std::cerr << "[ChunkIO][loadBlocks]: Read error for block name.\n";
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
			if (!reader.read(&indicesCount))
			{
				std::cerr << "[ChunkIO][loadBlocks]: Read error for indices count.\n";
				blockChanges.clear();
				return;
			}
			
			if (indicesCount == 0 || indicesCount > CHUNK_VOLUME)
			{
				std::cerr << "[ChunkIO][loadBlocks]: Indices count is invalid.\n";
				blockChanges.clear();
				return;
			}

			// Read indices
			std::vector<uint16_t> indices;
			indices.resize(indicesCount);
			if (!reader.read(indices.data(), indicesCount))
			{
				std::cerr << "[ChunkIO][loadBlocks]: Read error for indices.\n";
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

	// Compute hash value
	uint64_t hashValue = computeHash(blockChanges);

	std::string name = std::format("{}_{}_{}.bin", chunkPosition.x, chunkPosition.y, chunkPosition.z);
	fs::path filepath = CHUNK_SAVES_PATH / name;

	{
		// Check if file exists
		if (fs::exists(filepath) && fs::is_regular_file(filepath))
		{
			// Open file for reading
			StreamReader reader(filepath);
			if (!reader)
			{
				std::cerr << "[ChunkIO][saveBlocks]: Failed to open file.\n";
			}
			else
			{
				// Read hash value
				uint64_t storedHasValue;
				if (!reader.read(&storedHasValue))
				{
					std::cerr << "[ChunkIO][saveBlocks]: Read error for hash value.\n";
					return;
				}

				// Compare hash value
				if (hashValue == storedHasValue)
				{
					return;
				}
			}
		}
	}

	// Open file for writing
	StreamWriter writer(filepath);
	if (!writer)
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to create file.\n";
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

	// Write hash value
	if (!writer.write(&hashValue))
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to write has value.\n";
		return;
	}

	// Write pack count
	uint16_t packCount = static_cast<uint16_t>(packs.size());
	if (!writer.write(&packCount))
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to write pack count.\n";
		return;
	}

	// Write each pack and its blocks with indices immediately after each block
	for (const auto& pack : packs)
	{
		// Write block count for this pack
		uint16_t blockCount = static_cast<uint16_t>(pack.blocks.size());
		if (!writer.write(&blockCount))
		{
			std::cerr << "[ChunkIO][saveBlocks]: Failed to write block count.\n";
			return;
		}

		// Write pack name
		uint8_t packNameLen = static_cast<uint8_t>(pack.name.size());
		if (!writer.write(&packNameLen) ||
			!writer.writeBytes(pack.name.data(), packNameLen))
		{
			std::cerr << "[ChunkIO][saveBlocks]: Failed to write pack name.\n";
			return;
		}

		// Write each block in pack
		for (const auto& [globalID, blockName] : pack.blocks)
		{
			// Write block name
			uint8_t blockNameLen = static_cast<uint8_t>(blockName.size());
			if (!writer.write(&blockNameLen) ||
				!writer.writeBytes(blockName.data(), blockNameLen))
			{
				std::cerr << "[ChunkIO][saveBlocks]: Failed to write block name.\n";
				return;
			}

			// Write indices for this block
			const auto& indices = blockChanges.at(globalID);
			uint16_t indicesCount = static_cast<uint16_t>(indices.size());
			if (!writer.write(&indicesCount) ||
				(indicesCount > 0 && !writer.write(indices.data(), indicesCount)))
			{
				std::cerr << "[ChunkIO][saveBlocks]: Failed to write indices.\n";
				return;
			}
		}
	}

	// Ensure everything is written to disk
	if (!writer.flush())
	{
		std::cerr << "[ChunkIO][saveBlocks]: Failed to flush data to disk.\n";
	}
}