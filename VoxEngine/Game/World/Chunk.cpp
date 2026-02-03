#include "Chunk.h"
#include "Chunk/TerrainGenerator.h"
#include "Chunk/ChunkMeshManager.h"

#include "Game/DataPackManagment/AssetRegistry.h"

#include "Core/Profiler.h"
#include "Core/ASSERT.h"
#include "Core/Hashes/ivec2Hasher.h"
#include "Core/MemoryFileReader.h"
#include "Core/MemoryFileWriter.h"

#include <vector>
#include <format>

thread_local ChunkSpecializedQueue<LightNode> Chunk::localBlockLightBfsQueue;
thread_local ChunkSpecializedQueue<LightRemovalNode> Chunk::localBlockLightRemovalBfsQueue;
thread_local ChunkSpecializedQueue<LightNode> Chunk::localSkyLightBfsQueue;
thread_local ChunkSpecializedQueue<LightRemovalNode> Chunk::localSkyLightRemovalBfsQueue;
std::vector<ChunkMeshData*> Chunk::pendingMeshUploads;
StructureBlockChangeManager Chunk::structureBlockChangeManager;
std::filesystem::path Chunk::CHUNK_SAVES_PATH;

int hash3(unsigned x, unsigned y, unsigned z)
{
	unsigned data = x * 0x27d4eb2du + y * 0x165667b1u + z * 0x1b873593u;
	data ^= data >> 15u;
	data *= 0x85ebca6bu;
	data ^= data >> 13u;
	data *= 0xc2b2ae35u;
	data ^= data >> 16u;
	return data;
}

Chunk::Chunk()
{
}

Chunk::~Chunk()
{
	saveBlocks();
}

inline bool Chunk::operator==(const Chunk& other) const
{
	return position == other.position;
}

// Prepares chunk for use
void Chunk::init(const glm::ivec3& position, Chunk** neighbors)
{
	// Set position
	this->position = position;

	// Set neighbours
	for (int i = 0; i < 6; i++)
	{
		Chunk* neighbor = neighbors[i];
		this->neighbors[i] = neighbor;
		if (neighbor)
		{
			neighbor->neighbors[i ^ 1] = this;
		}
	}

	// Reset
	ASSERT(loaderCount > 0);

	setState(Chunk::State::NotInitialized_NeedsBlocks);

	chunkFlags.reset();
	chunkFlags.set(Flag::IsLoadedInWorld, true);

	meshData.resetRenderFaceCount();
	meshData.dirty = false;

	// Should be separate from meshData.dirty
	meshDirty = false;

	enabledAlignedOpaqueFaceSides.fill(true);
	
	ASSERT(changedBlocks.empty());
}

// Cleans up resources
void Chunk::destroy()
{
	// Clear neighbors
	for (int i = 0; i < 6; i++)
	{
		Chunk* neighbor = neighbors[i];
		if (neighbor)
		{
			neighbor->neighbors[i ^ 1] = nullptr;
			neighbors[i] = nullptr;
		}
	}

	//
	ASSERT(loaderCount == 0);

	//
	setState(Chunk::State::NotInitialized_NeedsBlocks);

	//
	chunkFlags.set(Flag::IsLoadedInWorld, false);
	meshData.clearInstances();

	// Release chunk column data
	if (chunkFlags.read(Flag::IsLoadedChunkColumnData))
	{
		chunkFlags.set(Flag::IsLoadedChunkColumnData, false);
		TerrainGenerator::getInstance().unloadChunkColumnData(position.x, position.z);
	}

	// Clear light BFS queues
	{
		std::lock_guard<std::mutex> lock(blockLightBfsMutex);
		blockLightBfsQueue.clear();
	}
	{
		std::lock_guard<std::mutex> lock(blockLightRemovalBfsMutex);
		blockLightRemovalBfsQueue.clear();
	}
	{
		std::lock_guard<std::mutex> lock(skyLightBfsMutex);
		skyLightBfsQueue.clear();
	}
	{
		std::lock_guard<std::mutex> lock(skyLightRemovalBfsMutex);
		skyLightRemovalBfsQueue.clear();
	}

	// TODO: Make it async. Mark chunk as processing.
	saveBlocks();
	changedBlocks.clear();
}

void Chunk::globalInit()
{
	localBlockLightBfsQueue.reserve(CHUNK_VOLUME);
	localBlockLightRemovalBfsQueue.reserve(CHUNK_VOLUME);
	localSkyLightBfsQueue.reserve(CHUNK_VOLUME);
	localSkyLightRemovalBfsQueue.reserve(CHUNK_VOLUME);
}

void Chunk::buildBlocks()
{
	if (
		areBlocksBuilt() ||
		!chunkFlags.read(Flag::IsLoadedInWorld)
	)
	{
		return;
	}

	ScopedProcessingFence scopedFence(processingFence);

	// Load chunk column data
	const ChunkColumnData* chunkColumnData;
	chunkColumnData = TerrainGenerator::getInstance().loadChunkColumnData(position.x, position.z);
	chunkFlags.set(Flag::IsLoadedChunkColumnData, true);
	const int* heightMap = chunkColumnData->heightMapRead();

	// Fetch IDs
	// TODO: Maybe fetch block ids once and make them Chunk's static variables?
	const BlockId airID = AssetRegistry::getBlockNumericalId("core:air");
	const BlockId waterID = AssetRegistry::getBlockNumericalId("core:water");
	const BlockId grassBlockId = AssetRegistry::getBlockNumericalId("core:grass_block");
	const BlockId dirtID = AssetRegistry::getBlockNumericalId("core:dirt");
	const BlockId stoneID = AssetRegistry::getBlockNumericalId("core:stone");

	// Terrain
	bool computeCaveMask = false;
	{
		PROFILE_SCOPE("Build chunk blocks", ProfileCategory::ChunkBlocks);

		const int globalChunkY = position.y * CHUNK_SIZE;
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				int globalHeight = heightMap[z + x * CHUNK_SIZE];
				for (int y = 0; y < CHUNK_SIZE; y++)
				{
					int globalY = globalChunkY + y;
					bool ocean = globalY <= 0;

					size_t index = getIndex(x, y, z);

					BlockId block;
					if (globalY > globalHeight)
					{
						block = ocean ? waterID : airID;
					}
					else if (globalY == globalHeight)
					{
						block = grassBlockId;
					}
					else if (globalY > globalHeight - 4)
					{
						block = dirtID;
					}
					else
					{
						block = stoneID;
						computeCaveMask = true;
					}

					blocks[index] = block;
				}
			}
		}
	}

	// Caves
	if (computeCaveMask)
	{
		bool caveMask[CHUNK_VOLUME];
		TerrainGenerator::getInstance().computeCaveMask(caveMask, position.x, position.y, position.z);

		PROFILE_SCOPE("Generate caves", ProfileCategory::ChunkBlocks);

		for (int i = 0; i < CHUNK_VOLUME; i++)
		{
			if (blocks[i] == stoneID && caveMask[i])
			{
				blocks[i] = airID;
			}
		}
	}

	// Trees
	{
		PROFILE_SCOPE("Generate trees", ProfileCategory::ChunkBlocks);
	
		const ivec2Hasher hasher;

		const glm::ivec3 globalChunkPosition = position * CHUNK_SIZE;

		const glm::ivec2 globalChunkXZ = { globalChunkPosition.x, globalChunkPosition.z };

		for (int x = 0; x < CHUNK_SIZE; x += 2)
		{
			for (int z = 0; z < CHUNK_SIZE; z += 2)
			{
				int treeRootHeight = heightMap[z + x * CHUNK_SIZE] + 1;
				int localY = treeRootHeight - globalChunkPosition.y;

				if (localY < 0 || localY >= CHUNK_SIZE)
				{
					continue;
				}

				size_t rootIndex = getIndex(x, localY, z);
				if (blocks[rootIndex] != airID)
				{
					continue;
				}

				glm::ivec2 worldPos = globalChunkXZ + glm::ivec2(x, z);

				size_t hashValue = hasher(worldPos);

				if ((hashValue % 100) >= 2)
				{
					continue;
				}

				generateTree({ x, localY, z });
			}
		}
	}

	// Incoming structures
	{
		PROFILE_SCOPE("Apply incoming structural changes", ProfileCategory::ChunkBlocks);

		auto pendingChanges = structureBlockChangeManager.retrieveAndClearChanges(position);
		for (const auto& change : pendingChanges)
		{
			if (!change.placeIfBlockIsAir || blocks[change.index] == airID)
			{
				blocks[change.index] = change.block;
			}
		}
	}

	// Load blocks
	loadBlocks();

	//
	setState(State::BlocksBuilt);

	// Mark itself and neighbors meshes as dirty

	{
		meshDirty = true;

		// Sides
		Chunk* n0;
		if (n0 = neighbors[0]) n0->meshDirty = true;
		if (n0 = neighbors[1]) n0->meshDirty = true;
		if (n0 = neighbors[2]) n0->meshDirty = true;
		if (n0 = neighbors[3]) n0->meshDirty = true;
		if (n0 = neighbors[4]) n0->meshDirty = true;
		if (n0 = neighbors[5]) n0->meshDirty = true;

		// Edges
		Chunk* n1;
		if ((n0 = neighbors[0]) && (n1 = n0->neighbors[2])) n1->meshDirty = true;
		if ((n0 = neighbors[0]) && (n1 = n0->neighbors[3])) n1->meshDirty = true;
		if ((n0 = neighbors[1]) && (n1 = n0->neighbors[2])) n1->meshDirty = true;
		if ((n0 = neighbors[1]) && (n1 = n0->neighbors[3])) n1->meshDirty = true;
		if ((n0 = neighbors[0]) && (n1 = n0->neighbors[4])) n1->meshDirty = true;
		if ((n0 = neighbors[0]) && (n1 = n0->neighbors[5])) n1->meshDirty = true;
		if ((n0 = neighbors[1]) && (n1 = n0->neighbors[4])) n1->meshDirty = true;
		if ((n0 = neighbors[1]) && (n1 = n0->neighbors[5])) n1->meshDirty = true;
		if ((n0 = neighbors[2]) && (n1 = n0->neighbors[4])) n1->meshDirty = true;
		if ((n0 = neighbors[2]) && (n1 = n0->neighbors[5])) n1->meshDirty = true;
		if ((n0 = neighbors[3]) && (n1 = n0->neighbors[4])) n1->meshDirty = true;
		if ((n0 = neighbors[3]) && (n1 = n0->neighbors[5])) n1->meshDirty = true;

		// Corners
		Chunk* n2;
		if ((n0 = neighbors[0]) &&
			(n1 = n0->neighbors[2]) &&
			(n2 = n1->neighbors[4]))
		{
			n2->meshDirty = true;
		}
		if ((n0 = neighbors[0]) &&
			(n1 = n0->neighbors[2]) &&
			(n2 = n1->neighbors[5]))
		{
			n2->meshDirty = true;
		}
		if ((n0 = neighbors[0]) &&
			(n1 = n0->neighbors[3]) &&
			(n2 = n1->neighbors[4]))
		{
			n2->meshDirty = true;
		}
		if ((n0 = neighbors[0]) &&
			(n1 = n0->neighbors[3]) &&
			(n2 = n1->neighbors[5]))
		{
			n2->meshDirty = true;
		}
		if ((n0 = neighbors[1]) &&
			(n1 = n0->neighbors[2]) &&
			(n2 = n1->neighbors[4]))
		{
			n2->meshDirty = true;
		}
		if ((n0 = neighbors[1]) &&
			(n1 = n0->neighbors[2]) &&
			(n2 = n1->neighbors[5]))
		{
			n2->meshDirty = true;
		}
		if ((n0 = neighbors[1]) &&
			(n1 = n0->neighbors[3]) &&
			(n2 = n1->neighbors[4]))
		{
			n2->meshDirty = true;
		}
		if ((n0 = neighbors[1]) &&
			(n1 = n0->neighbors[3]) &&
			(n2 = n1->neighbors[5]))
		{
			n2->meshDirty = true;
		}
	}
}

void Chunk::updateStructureBlocks()
{
	const BlockId airID = AssetRegistry::getBlockNumericalId("core:air");

	ScopedProcessingFence scopedFence(processingFence);

	auto pendingChanges = structureBlockChangeManager.retrieveAndClearChanges(position);
	for (const auto& change : pendingChanges)
	{
		if (!change.placeIfBlockIsAir || blocks[change.index] == airID)
		{
			auto pos = getPositionFromIndex(change.index);
			setBlockAt(pos.x, pos.y, pos.z, change.block, false);
		}
	}
}

void Chunk::generateTree(const glm::ivec3& rootPosition)
{
	const BlockId airID = AssetRegistry::getBlockNumericalId("core:air");
	const BlockId logID = AssetRegistry::getBlockNumericalId("core:oak_log");
	const BlockId leavesID = AssetRegistry::getBlockNumericalId("core:oak_leaves");

	const int treeHeight = 4;

	// Check if there's enough space for the tree
	if (rootPosition.y + treeHeight + 3 >= CHUNK_SIZE)
	{
		return; // Not enough vertical space
	}

	// Trunk
	for (int i = 0; i < treeHeight; i++)
	{
		size_t index = getIndex(rootPosition.x, rootPosition.y + i, rootPosition.z);
		blocks[index] = logID;
	}
	
	// Leaves - create a spherical canopy
	int leavesStart = rootPosition.y + treeHeight - 2;
	int leavesEnd = rootPosition.y + treeHeight + 2;

	for (int ly = leavesStart; ly <= leavesEnd; ly++)
	{
		for (int lx = rootPosition.x - 2; lx <= rootPosition.x + 2; lx++)
		{
			for (int lz = rootPosition.z - 2; lz <= rootPosition.z + 2; lz++)
			{
				// Create spherical leaf pattern
				int dx = lx - rootPosition.x;
				int dz = lz - rootPosition.z;
				int dy = ly - (rootPosition.y + treeHeight);

				float distance = sqrtf(dx * dx + dz * dz + dy * dy * 0.8f); // Slightly elliptical

				if (distance > 2.0f)
				{
					continue;
				}

				if (((lx | ly | lz) & CHUNK_UPPER_BITS_MASK) == 0)
				{
					size_t index = getIndex(lx, ly, lz);
					if (blocks[index] == airID)
					{
						blocks[index] = leavesID;
					}
				}
				else
				{
					glm::ivec3 chunkPos = position;

					if (lx < 0) chunkPos.x--;
					else if (lx >= CHUNK_SIZE) chunkPos.x++;

					if (ly < 0) chunkPos.y--;
					else if (ly >= CHUNK_SIZE) chunkPos.y++;

					if (lz < 0) chunkPos.z--;
					else if (lz >= CHUNK_SIZE) chunkPos.z++;

					int nx = lx & CHUNK_LOWER_BITS_MASK;
					int ny = ly & CHUNK_LOWER_BITS_MASK;
					int nz = lz & CHUNK_LOWER_BITS_MASK;
					size_t index = getIndex(nx, ny, nz);

					structureBlockChangeManager.addChange(chunkPos, leavesID, index, true);
				}
			}
		}
	}
}

void Chunk::loadBlocks()
{
	PROFILE_SCOPE("Load chunk blocks", ProfileCategory::ChunkBlocks);

	namespace fs = std::filesystem;
	std::string name = std::format("{}_{}_{}.bin", position.x, position.y, position.z);
	fs::path filepath = CHUNK_SAVES_PATH / name;

	if (!fs::exists(filepath) || !fs::is_regular_file(filepath))
	{
		return;
	}

	constexpr size_t MAX_PACKS = CHUNK_VOLUME;
	constexpr size_t MIN_FILE_SIZE = 2 + 2 + 1 + 1 + 1 + 1 + 2 + 2;
	constexpr size_t MAX_FILE_SIZE = 2 + (MAX_PACKS * (2 + 1 + 64)) +
		CHUNK_VOLUME * (1 + 64 + 2 + 2);

	MemoryFileReader file;
	auto loadResult = file.loadFile(filepath, MAX_FILE_SIZE);
	if (loadResult != MemoryFileReader::Result::Success)
	{
		std::cerr << "[Chunk][loadBlocks]: Failed to open file.\n";
		return;
	}

	// Check file size
	if (file.getSize() < MIN_FILE_SIZE)
	{
		std::cerr << "[Chunk][loadBlocks]: File too small.\n";
		return;
	}

	// Get air block ID
	const BlockId AIR_BLOCK_ID = AssetRegistry::getBlockNumericalId("core:air");

	// Read pack count
	uint16_t packCount = 0;
	auto readResult = file.read(&packCount);
	if (readResult != MemoryFileReader::Result::Success)
	{
		std::cerr << "[Chunk][loadBlocks]: Read error.\n";
		return;
	}
	else if (packCount == 0 || packCount > MAX_PACKS)
	{
		std::cerr << "[Chunk][loadBlocks]: Pack count is invalid.\n";
		return;
	}

	// Clear existing changes
	changedBlocks.clear();

	// Read all packs, blocks, and their indices
	for (uint16_t packIndex = 0; packIndex < packCount; packIndex++)
	{
		// Read block count for this pack
		uint16_t packBlockCount = 0;
		readResult = file.read(&packBlockCount);
		if (readResult != MemoryFileReader::Result::Success)
		{
			std::cerr << "[Chunk][loadBlocks]: Read error.\n";
			changedBlocks.clear();
			return;
		}
		else if (packBlockCount == 0 || packBlockCount > CHUNK_VOLUME)
		{
			std::cerr << "[Chunk][loadBlocks]: Block count in pack is invalid.\n";
			changedBlocks.clear();
			return;
		}

		// Read pack name
		uint8_t packNameLen = 0;
		readResult = file.read(&packNameLen);
		if (readResult != MemoryFileReader::Result::Success)
		{
			std::cerr << "[Chunk][loadBlocks]: Read error.\n";
			changedBlocks.clear();
			return;
		}
		else if (packNameLen < 1 || packNameLen > 64)
		{
			std::cerr << "[Chunk][loadBlocks]: Pack name length is invalid.\n";
			changedBlocks.clear();
			return;
		}

		std::string packName(packNameLen, '\0');
		readResult = file.read(&packName[0], packNameLen);
		if (readResult != MemoryFileReader::Result::Success)
		{
			std::cerr << "[Chunk][loadBlocks]: Read error.\n";
			changedBlocks.clear();
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
				std::cerr << "[Chunk][loadBlocks]: Read error.\n";
				changedBlocks.clear();
				return;
			}
			else if (blockNameLen < 1 || blockNameLen > 64)
			{
				std::cerr << "[Chunk][loadBlocks]: Block name length is invalid.\n";
				changedBlocks.clear();
				return;
			}

			// Read block name
			std::string blockName(blockNameLen, '\0');
			readResult = file.read(&blockName[0], blockNameLen);
			if (readResult != MemoryFileReader::Result::Success)
			{
				std::cerr << "[Chunk][loadBlocks]: Read error.\n";
				changedBlocks.clear();
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
				std::cerr << "[Chunk][loadBlocks]: Block '" << fullName << "' is not found. Replaced with air.\n";
			}

			// Read indices count for this block
			uint16_t indicesCount = 0;
			readResult = file.read(&indicesCount);
			if (readResult != MemoryFileReader::Result::Success)
			{
				std::cerr << "[Chunk][loadBlocks]: Read error.\n";
				changedBlocks.clear();
				return;
			}
			else if (indicesCount == 0 || indicesCount > CHUNK_VOLUME)
			{
				std::cerr << "[Chunk][loadBlocks]: Indices count is invalid.\n";
				changedBlocks.clear();
				return;
			}

			// Read indices (empty count is allowed for removed blocks)
			std::vector<uint16_t> indices;
			indices.resize(indicesCount);
			readResult = file.read(indices.data(), indicesCount);
			if (readResult != MemoryFileReader::Result::Success)
			{
				std::cerr << "[Chunk][loadBlocks]: Read error.\n";
				changedBlocks.clear();
				return;
			}

			// Store in changedBlocks map
			changedBlocks[globalID] = std::move(indices);
		}
	}

	if (changedBlocks.empty())
	{
		std::cerr << "[Chunk][loadBlocks]: No blocks loaded.\n";
		return;
	}

	// Verify we read the entire file
	if (!file.isEndOfFile())
	{
		std::cerr << "[Chunk][loadBlocks]: File size mismatch. Possible corruption.\n";
		changedBlocks.clear();
		return;
	}

	// Apply loaded data to blocks array
	for (auto it = changedBlocks.begin(); it != changedBlocks.end();)
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
			it = changedBlocks.erase(it);
		}
		else
		{
			indices.resize(writeIndex);
			++it;
		}
	}
}

void Chunk::saveBlocks()
{
	if (changedBlocks.empty())
	{
		return;
	}
	else if (changedBlocks.size() > CHUNK_VOLUME)
	{
		std::cerr << "[Chunk][saveBlocks]: ChangedBlocks map size is invalid.\n";
		return;
	}

	PROFILE_SCOPE("Save chunk blocks", ProfileCategory::ChunkBlocks);

	namespace fs = std::filesystem;
	std::string name = std::format("{}_{}_{}.bin", position.x, position.y, position.z);
	fs::path filepath = CHUNK_SAVES_PATH / name;

	constexpr size_t MAX_PACKS = CHUNK_VOLUME;
	constexpr size_t MAX_FILE_SIZE = 2 + (MAX_PACKS * (2 + 1 + 64)) +
		CHUNK_VOLUME * (1 + 64 + 2 + 2);

	MemoryFileWriter file;
	auto initResult = file.initialize(MAX_FILE_SIZE);
	if (initResult != MemoryFileWriter::Result::Success)
	{
		std::cerr << "[Chunk][saveBlocks]: MemoryFileWriter failed to initialize.\n";
		return;
	}

	// Filter valid changes and collect block names
	std::map<BlockId, std::string> idToString;
	{
		const BlockData* blockData = nullptr;
		auto it = changedBlocks.begin();
		while (it != changedBlocks.end())
		{
			if (filterChanges(it->first, it->second, blockData))
			{
				it = changedBlocks.erase(it);
			}
			else
			{
				idToString[it->first] = blockData->stringId;
				++it;
			}
		}
	}
	ASSERT(idToString.size() == changedBlocks.size());
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
			const auto& indices = changedBlocks.at(globalID);
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
		std::cerr << "[Chunk][saveBlocks]: Failed to save file: " << static_cast<int>(saveResult)<< "\n";
		return;
	}
}

bool Chunk::filterChanges(BlockId BlockId, const std::vector<uint16_t>& indices, const BlockData*& outBlockData) const
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

bool Chunk::findFloodFillStartIndex(uint16_t& startIndex, const bool* floodFillMask) const
{
	//for (uint16_t i = startIndex; i < CHUNK_VOLUME; i++)
	//{
	//	if (floodFillMask[i])
	//	{
	//		// Already visited
	//		continue;
	//	}

	//	Block block = blocks[i];
	//	const BlockData* blockData = BlockRegistry::getBlockData(block);
	//	if (!blockData->properties.areFacesTransparent)
	//	{
	//		// Block isn't transparent
	//		continue;
	//	}

	//	startIndex = i;
	//	return true;
	//}
	//return false;
	return false;
}

void Chunk::computeConnectivity()
{
	//PROFILE_SCOPE("Compute chunk connectivity", ProfileCategory::General);

	//constexpr glm::ivec3 dirs[6] =
	//{
	//	{-1, 0, 0}, {1, 0, 0},
	//	{0, -1, 0}, {0, 1, 0},
	//	{0, 0, -1}, {0, 0, 1}
	//};

	//// Reset regions
	//bool visitedCells[CHUNK_VOLUME]; // TODO: Can be a bitset
	//std::fill(visitedCells, visitedCells + CHUNK_VOLUME, false);

	//SymmetricBitMatrix<6> chunkConnectivity; // 6x6 matrix
	//chunkConnectivity.fill(false);

	//uint16_t startIndex = 0;

	//std::vector<glm::ivec3> cellsToVisit;

	////static std::mutex mtx;
	////std::lock_guard<std::mutex> lock(mtx);

	//while (true)
	//{
	//	// Find start index
	//	if (!findFloodFillStartIndex(startIndex, visitedCells))
	//	{
	//		break;
	//	}

	//	glm::ivec3 startPos = getPositionFromIndex(startIndex);
	//	visitedCells[startIndex] = true; // Mark as visited
	//	startIndex++; // Increment, so 'findFloodFillStartIndex' will look immediately at next block

	//	bool regionConnectivity[6] = { false, false, false, false, false, false }; // TODO: Can be a bitset

	//	cellsToVisit.push_back(startPos);
	//	while (!cellsToVisit.empty())
	//	{
	//		// Get cell
	//		glm::ivec3 cell = cellsToVisit.back();
	//		cellsToVisit.pop_back();

	//		// Check if cell is on chunk border
	//		regionConnectivity[0] |= cell.x == 0;
	//		regionConnectivity[1] |= cell.x == (CHUNK_SIZE - 1);
	//		regionConnectivity[2] |= cell.y == 0;
	//		regionConnectivity[3] |= cell.y == (CHUNK_SIZE - 1);
	//		regionConnectivity[4] |= cell.z == 0;
	//		regionConnectivity[5] |= cell.z == (CHUNK_SIZE - 1);
	//	
	//		// Spread neighbors
	//		for (int i = 0; i < 6; i++)
	//		{
	//			glm::ivec3 neighborPos = cell + dirs[i];
	//			
	//			// Check if neighbor is in boundaries
	//			glm::ivec3 truncated = neighborPos & CHUNK_UPPER_BITS_MASK;
	//			if (!(truncated.x == 0 && truncated.y == 0 && truncated.z == 0))
	//			{
	//				continue;
	//			}
	//			size_t neighborIndex = getIndex(neighborPos.x, neighborPos.y, neighborPos.z);

	//			// Check if neighbor is visited
	//			if (visitedCells[neighborIndex])
	//			{
	//				continue;
	//			}
	//			visitedCells[neighborIndex] = true;

	//			// Check if neighbor is in opaque block
	//			Block block = blocks[neighborIndex];
	//			if (!GET_BLOCK_PROPERTIES(block).areFacesTransparent)
	//			{
	//				continue;
	//			}

	//			cellsToVisit.push_back(neighborPos);
	//		}
	//	}

	//	// Region is filled
	//	for (int i = 0; i < 5; i++)
	//	{
	//		for (int j = i + 1; j < 6; j++)
	//		{
	//			chunkConnectivity.set(i, j, true);
	//		}
	//	}
	//}

	//// Check flood fill mask if it filled all the space
}

void Chunk::removeIndexFromMap(BlockId block, uint16_t idx)
{
	auto it = changedBlocks.find(block);
	if (it == changedBlocks.end()) return;

	auto& vec = it->second;

	for (size_t i = 0; i < vec.size(); i++)
	{
		if (vec[i] == idx)
		{
			vec[i] = vec.back();  // swap with last
			vec.pop_back();       // remove last
			break;
		}
	}

	if (vec.empty()) changedBlocks.erase(it);
}

// TODO: Underground light is wrong on borders. Sky light is not zero somehow. Problem is in mesh update, not in light propagation.
// Maybe it is because of chunk's on edge can't be getten for 100% sure.
// Either look-up directly in map, but then we will should use mutex, or try more ways to reach certain chunks.
// 
// TODO: Cache neighbor meshes dirty value and set it once
void Chunk::buildLight()
{
	if (
		isLightBuilt() ||
		!areBlocksBuilt() ||
		!chunkFlags.read(Flag::IsLoadedInWorld)
		)
	{
		return;
	}

	ScopedProcessingFence scopedFence(processingFence);

	PROFILE_SCOPE("Build chunk light", ProfileCategory::ChunkLight);

	const ChunkColumnData* chunkColumnData = TerrainGenerator::getInstance().getChunkColumnData(position.x, position.z);
	const int* heightMap = chunkColumnData->heightMapRead();
	const Chunk* top = neighbors[3];

	// Constants for direction offsets
	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };

	// Initialize all light values to 0
	std::fill(std::begin(lightLevels), std::end(lightLevels), LightLevel(0, 0));

	// Step 1: Collect block light sources
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					size_t index = getIndex(x, y, z);

					const auto* blockData = AssetRegistry::getBlockData(blocks[index]);
					if (!blockData)
					{
						continue;
					}

					uint8_t emission = blockData->lightEmission;
					if (emission == 0)
					{
						continue;
					}

					setBlockLightAt(x, y, z, emission);
					localBlockLightBfsQueue.emplace(x, y, z);
				}
			}
		}
	}

	// Step 2: Collect sky light sources
	{
		if (top && top->isLightBuilt())
		{
			// Propagate from top neighbor
			const int y = CHUNK_SIZE - 1;
			const int neighborY = 0;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					size_t index = getIndex(x, y, z);
					const auto* blockData = AssetRegistry::getBlockData(blocks[index]);
					if (blockData && blockData->absorbsLight)
					{
						continue;
					}
					uint8_t neighborSkyLight = top->getLightAt(x, neighborY, z).skyLight;

					if (neighborSkyLight == 0)
					{
						continue;
					}

					uint8_t newSkyLight = neighborSkyLight < 15 ? neighborSkyLight - 1 : neighborSkyLight;

					setSkyLightAt(x, y, z, neighborSkyLight);
					localSkyLightBfsQueue.emplace(x, y, z);
				}
			}
		}
		else
		{
			const int globalChunkY = position.y * CHUNK_SIZE;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					int globalHeight = heightMap[z + x * CHUNK_SIZE];
					for (int y = CHUNK_SIZE - 1; y >= 0; y--)
					{
						int globalY = globalChunkY + y;
						size_t index = getIndex(x, y, z);
						if (globalY > globalHeight)
						{
							// Air block
							setSkyLightAt(x, y, z, 15);
							localSkyLightBfsQueue.emplace(x, y, z);
						}
						else
						{
							// Reached terrain
							break;
						}
					}
				}
			}
		}
	}

	// Step 3: Collect light from neighbors
	// TODO: If neighbor block is solid, then check if it's a light source and propagate from it
	{
		auto processNeighborFace = [&](int x, int y, int z, int nx, int ny, int nz, const Chunk* neighbor, bool propagatingFromTop)
			{
				size_t index = getIndex(x, y, z);
				const auto* blockData = AssetRegistry::getBlockData(blocks[index]);
				if (blockData && blockData->absorbsLight)
				{
					return;
				}

				LightLevel neighborLight = neighbor->getLightAt(nx, ny, nz);

				// Block light
				if (lightLevels[index].blockLight + 1 < neighborLight.blockLight)
				{
					setBlockLightAt(x, y, z, neighborLight.blockLight - 1);
					localBlockLightBfsQueue.emplace(x, y, z);
				}

				// Sky light
				uint8_t skyLightAbsorption = (propagatingFromTop && lightLevels[index].skyLight == 15) ? 0 : 1;
				if (lightLevels[index].skyLight + skyLightAbsorption < neighborLight.skyLight)
				{
					setSkyLightAt(x, y, z, neighborLight.skyLight - skyLightAbsorption);
					localSkyLightBfsQueue.emplace(x, y, z);
				}
			};

		// -X
		const Chunk* neighbor = neighbors[0];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int x = 0;
			const int neighborX = CHUNK_SIZE - 1;
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					processNeighborFace(x, y, z, neighborX, y, z, neighbor, false);
				}
			}
		}

		// +X
		neighbor = neighbors[1];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int x = CHUNK_SIZE - 1;
			const int neighborX = 0;
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					processNeighborFace(x, y, z, neighborX, y, z, neighbor, false);
				}
			}
		}

		// -Y
		neighbor = neighbors[2];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int y = 0;
			const int neighborY = CHUNK_SIZE - 1;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					processNeighborFace(x, y, z, x, neighborY, z, neighbor, false);
				}
			}
		}

		// +Y
		neighbor = neighbors[3];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int y = CHUNK_SIZE - 1;
			const int neighborY = 0;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					processNeighborFace(x, y, z, x, neighborY, z, neighbor, true);
				}
			}
		}

		// -Z
		neighbor = neighbors[4];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int z = 0;
			const int neighborZ = CHUNK_SIZE - 1;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int y = 0; y < CHUNK_SIZE; y++)
				{
					processNeighborFace(x, y, z, x, y, neighborZ, neighbor, false);
				}
			}
		}

		// +Z
		neighbor = neighbors[5];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int z = CHUNK_SIZE - 1;
			const int neighborZ = 0;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int y = 0; y < CHUNK_SIZE; y++)
				{
					processNeighborFace(x, y, z, x, y, neighborZ, neighbor, false);
				}
			}
		}
	}

	// Step 4: Propagate block light using flood-fill
	{
		while (!localBlockLightBfsQueue.empty())
		{
			// Get node data
			const auto data = localBlockLightBfsQueue.pop_and_return_unsafe();

			// Get light level
			uint8_t blockLight = lightLevels[getIndex(data.x, data.y, data.z)].blockLight;
			if (blockLight < 2)
			{
				continue;
			}
			uint8_t lightToSet = blockLight - 1;

			// Propagate to neighbors
			for (int i = 0; i < 6; i++)
			{
				int nx = data.x + dx[i];
				int ny = data.y + dy[i];
				int nz = data.z + dz[i];

				size_t neighborIndex;
				Chunk* neighborChunk = getChunkAndIndex_checkSideNeighbor(nx, ny, nz, i, neighborIndex);
				if (!neighborChunk)
				{
					continue;
				}

				const BlockId neighborBlock = neighborChunk->getBlockAt(neighborIndex);
				const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
				if (neighborBlockData && neighborBlockData->absorbsLight)
				{
					continue;
				}

				uint8_t neighborBlockLight = neighborChunk->getLightAt(neighborIndex).blockLight;
				if (neighborBlockLight >= lightToSet)
				{
					continue;
				}

				if (neighborChunk == this)
				{
					setBlockLightAt(nx, ny, nz, lightToSet);
					localBlockLightBfsQueue.emplace(nx, ny, nz);
				}
				else
				{
					int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

					neighborChunk->setBlockLightAt(neighborIndex, lightToSet);
					neighborChunk->addBlockLightNodeToQueue(neighborNX, neighborNY, neighborNZ);
				}
			}
		}
	}

	// Step 5: Propagate sky light using flood-fill
	{
		while (!localSkyLightBfsQueue.empty())
		{
			// Get node data
			const auto data = localSkyLightBfsQueue.pop_and_return_unsafe();

			// Get light level
			uint8_t skyLight = lightLevels[getIndex(data.x, data.y, data.z)].skyLight;
			if (skyLight < 2)
			{
				continue;
			}

			// Propagate to neighbors
			for (int i = 0; i < 6; i++)
			{
				int nx = data.x + dx[i];
				int ny = data.y + dy[i];
				int nz = data.z + dz[i];

				size_t neighborIndex;
				Chunk* neighborChunk = getChunkAndIndex_checkSideNeighbor(nx, ny, nz, i, neighborIndex);
				if (!neighborChunk)
				{
					continue;
				}

				const BlockId neighborBlock = neighborChunk->getBlockAt(neighborIndex);
				const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
				if (neighborBlockData && neighborBlockData->absorbsLight)
				{
					continue;
				}

				uint8_t neighborSkyLight = neighborChunk->getLightAt(neighborIndex).skyLight;

				// If we are propagating down and skyLight is 15, lightAbsorption is 0, otherwise 1 
				uint8_t lightAbsorption = (i == 2 && skyLight == 15) ? 0 : 1;
				uint8_t lightToSet = skyLight - lightAbsorption;

				if (neighborSkyLight >= lightToSet)
				{
					continue;
				}

				if (neighborChunk == this)
				{
					setSkyLightAt(nx, ny, nz, lightToSet);
					localSkyLightBfsQueue.emplace(nx, ny, nz);
				}
				else
				{
					int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

					neighborChunk->setSkyLightAt(neighborIndex, lightToSet);
					neighborChunk->addSkyLightNodeToQueue(neighborNX, neighborNY, neighborNZ);
				}
			}
		}
	}

	setState(State::LightsBuilt);
}

void Chunk::updateLight()
{
	if (
		!isLightBuilt() ||
		!chunkFlags.read(Flag::IsLoadedInWorld)
		)
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(blockLightBfsMutex);
		localBlockLightBfsQueue.swap(blockLightBfsQueue);
	}
	{
		std::lock_guard<std::mutex> lock(blockLightRemovalBfsMutex);
		localBlockLightRemovalBfsQueue.swap(blockLightRemovalBfsQueue);
	}
	{
		std::lock_guard<std::mutex> lock(skyLightBfsMutex);
		localSkyLightBfsQueue.swap(skyLightBfsQueue);
	}
	{
		std::lock_guard<std::mutex> lock(skyLightRemovalBfsMutex);
		localSkyLightRemovalBfsQueue.swap(skyLightRemovalBfsQueue);
	}

	ScopedProcessingFence scopedFence(processingFence);

	PROFILE_SCOPE("Update chunk light", ProfileCategory::ChunkLight);

	//
	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };

	// Remove block light
	while (!localBlockLightRemovalBfsQueue.empty())
	{
		// Get node data
		const auto data = localBlockLightRemovalBfsQueue.pop_and_return_unsafe();

		// Propagate to neighbors
		for (int i = 0; i < 6; i++)
		{
			int nx = data.x + dx[i];
			int ny = data.y + dy[i];
			int nz = data.z + dz[i];

			size_t neighborIndex;
			Chunk* neighborChunk = getChunkAndIndex_checkSideNeighbor(nx, ny, nz, i, neighborIndex);
			if (!neighborChunk)
			{
				continue;
			}

			BlockId neighborBlock = neighborChunk->getBlockAt(neighborIndex);
			const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
			if (neighborBlockData && neighborBlockData->absorbsLight)
			{
				continue;
			}

			uint8_t neighborBlockLight = neighborChunk->getLightAt(neighborIndex).blockLight;
			if (neighborBlockLight > 0 && neighborBlockLight < data.lightLevel)
			{
				if (neighborChunk == this)
				{
					setBlockLightAt(nx, ny, nz, 0);
					localBlockLightRemovalBfsQueue.emplace(nx, ny, nz, neighborBlockLight);
				}
				else
				{
					int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

					neighborChunk->setBlockLightAt(neighborNX, neighborNY, neighborNZ, 0);
					neighborChunk->addBlockLightRemovalNodeToQueue(neighborNX, neighborNY, neighborNZ, neighborBlockLight);
				}
			}
			else if (neighborBlockLight >= data.lightLevel)
			{
				if (neighborChunk == this)
				{
					localBlockLightBfsQueue.emplace(nx, ny, nz);
				}
				else
				{
					int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

					neighborChunk->addBlockLightNodeToQueue(neighborNX, neighborNY, neighborNZ);
				}
			}
		}
	}

	// Propagate block light
	while (!localBlockLightBfsQueue.empty())
	{
		// Get node data
		const auto data = localBlockLightBfsQueue.pop_and_return_unsafe();

		// Get light level
		uint8_t blockLight = lightLevels[getIndex(data.x, data.y, data.z)].blockLight;
		if (blockLight < 2)
		{
			continue;
		}
		uint8_t lightToSet = blockLight - 1;

		// Propagate to neighbors
		for (int i = 0; i < 6; i++)
		{
			int nx = data.x + dx[i];
			int ny = data.y + dy[i];
			int nz = data.z + dz[i];

			size_t neighborIndex;
			Chunk* neighborChunk = getChunkAndIndex_checkSideNeighbor(nx, ny, nz, i, neighborIndex);
			if (!neighborChunk)
			{
				continue;
			}

			BlockId neighborBlock = neighborChunk->getBlockAt(neighborIndex);
			const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
			if (neighborBlockData && neighborBlockData->absorbsLight)
			{
				continue;
			}

			uint8_t neighborBlockLight = neighborChunk->getLightAt(neighborIndex).blockLight;
			if (neighborBlockLight >= lightToSet)
			{
				continue;
			}

			if (neighborChunk == this)
			{
				setBlockLightAt(nx, ny, nz, lightToSet);
				localBlockLightBfsQueue.emplace(nx, ny, nz);
			}
			else
			{
				int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
				int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
				int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

				neighborChunk->setBlockLightAt(neighborNX, neighborNY, neighborNZ, lightToSet);
				neighborChunk->addBlockLightNodeToQueue(neighborNX, neighborNY, neighborNZ);
			}
		}
	}

	// Remove sky light
	while (!localSkyLightRemovalBfsQueue.empty())
	{
		// Get node data
		const auto data = localSkyLightRemovalBfsQueue.pop_and_return_unsafe();

		// Propagate to neighbors
		for (int i = 0; i < 6; i++)
		{
			int nx = data.x + dx[i];
			int ny = data.y + dy[i];
			int nz = data.z + dz[i];

			size_t neighborIndex;
			Chunk* neighborChunk = getChunkAndIndex_checkSideNeighbor(nx, ny, nz, i, neighborIndex);
			if (!neighborChunk)
			{
				continue;
			}

			BlockId neighborBlock = neighborChunk->getBlockAt(neighborIndex);
			const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
			if (neighborBlockData && neighborBlockData->absorbsLight)
			{
				continue;
			}

			uint8_t neighborSkyLight = neighborChunk->getLightAt(neighborIndex).skyLight;
			if (neighborSkyLight > 0 &&
				(neighborSkyLight < data.lightLevel || (data.lightLevel == 15 && i == 2)))
			{
				if (neighborChunk == this)
				{
					setSkyLightAt(nx, ny, nz, 0);
					localSkyLightRemovalBfsQueue.emplace(nx, ny, nz, neighborSkyLight);
				}
				else
				{
					int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

					neighborChunk->setSkyLightAt(neighborNX, neighborNY, neighborNZ, 0);
					neighborChunk->addSkyLightRemovalNodeToQueue(neighborNX, neighborNY, neighborNZ, neighborSkyLight);
				}
			}
			else if (neighborSkyLight >= data.lightLevel)
			{
				if (neighborChunk == this)
				{
					localSkyLightBfsQueue.emplace(nx, ny, nz);
				}
				else
				{
					int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

					neighborChunk->addSkyLightNodeToQueue(neighborNX, neighborNY, neighborNZ);
				}
			}
		}
	}

	// Propagate sky light
	while (!localSkyLightBfsQueue.empty())
	{
		// Get node data
		const auto data = localSkyLightBfsQueue.pop_and_return_unsafe();

		// Get light level
		uint8_t skyLight = lightLevels[getIndex(data.x, data.y, data.z)].skyLight;
		if (skyLight < 2)
		{
			continue;
		}

		// Propagate to neighbors
		for (int i = 0; i < 6; i++)
		{
			int nx = data.x + dx[i];
			int ny = data.y + dy[i];
			int nz = data.z + dz[i];

			size_t neighborIndex;
			Chunk* neighborChunk = getChunkAndIndex_checkSideNeighbor(nx, ny, nz, i, neighborIndex);
			if (!neighborChunk)
			{
				continue;
			}

			BlockId neighborBlock = neighborChunk->getBlockAt(neighborIndex);
			const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
			if (neighborBlockData && neighborBlockData->absorbsLight)
			{
				continue;
			}

			uint8_t neighborSkyLight = neighborChunk->getLightAt(neighborIndex).skyLight;

			// If we are propagating down and skyLight is 15, lightAbsorption is 0, otherwise 1
			uint8_t lightAbsorption = (i == 2 && skyLight == 15) ? 0 : 1;
			uint8_t lightToSet = skyLight - lightAbsorption;
			if (neighborSkyLight >= lightToSet)
			{
				continue;
			}

			if (neighborChunk == this)
			{
				setSkyLightAt(nx, ny, nz, lightToSet);
				localSkyLightBfsQueue.emplace(nx, ny, nz);
			}
			else
			{
				int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
				int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
				int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

				neighborChunk->setSkyLightAt(neighborNX, neighborNY, neighborNZ, lightToSet);
				neighborChunk->addSkyLightNodeToQueue(neighborNX, neighborNY, neighborNZ);
			}
		}
	}
}

bool Chunk::hasLightUpdates() const
{
	return
		!blockLightBfsQueue.empty() ||
		!blockLightRemovalBfsQueue.empty() ||
		!skyLightBfsQueue.empty() ||
		!skyLightRemovalBfsQueue.empty();
}

void Chunk::updateMesh()
{
	if (
		!chunkFlags.read(Flag::IsLoadedInWorld) ||
		!isLightBuilt() ||
		!meshDirty
		)
	{
		return;
	}

	meshDirty = false;
	
	ScopedProcessingFence scopedFence(processingFence);

	PROFILE_SCOPE("Update chunk mesh", ProfileCategory::ChunkMesh);

	// Collect visible faces
	{
		const int dx[] = { -1, 1, 0, 0, 0, 0 };
		const int dy[] = { 0, 0, -1, 1, 0, 0 };
		const int dz[] = { 0, 0, 0, 0, -1, 1 };

		static const BlockData::TextureSlot fallbackTextureSlot(0, BlockData::TextureSlot::TextureTransformation::None, false);

		ChunkMeshData::InstancesStorage newInstances; // TODO: Maybe it should be static thread_local?
		// Ofcourse we can just clear and then fill meshData.instacesStorage directly, but then processing fence should be activated before it, which I don't want to.
		// Maybe there's a way without processing fence.
		const int globalChunkX = position.x * CHUNK_SIZE;
		const int globalChunkY = position.y * CHUNK_SIZE;
		const int globalChunkZ = position.z * CHUNK_SIZE;
		for (size_t currentBlockIndex = 0; currentBlockIndex < CHUNK_VOLUME; currentBlockIndex++)
		{
			glm::ivec3 pos = getPositionFromIndex(currentBlockIndex);

			// Generate new faces for this block
			BlockId block = blocks[currentBlockIndex];
			const BlockData* blockData = AssetRegistry::getBlockData(block);
			if (!blockData || !blockData->hasFaces)
			{
				continue;
			}

			const auto& modelID = blockData->modelId;
			const auto& textureSlots = blockData->textureSlots;
			
			const auto* model = AssetRegistry::getBlockModelData(modelID);
			if (!model)
			{
				continue;
			}

			// TODO: Maybe translucent faces shouldn't be culled. Maybe they should be drawn using GL_LEQUAL for depth test.

			// Aligned faces
			if (!model->alignedFaces.empty())
			{
				const uint32_t hash = hash3(
					globalChunkX + pos.x,
					globalChunkY + pos.y,
					globalChunkZ + pos.z
				);
				const uint32_t transformationBitMasks[3] = { 0u, 0b11u, 0b111u };
				for (const auto& face : model->alignedFaces)
				{
					int nx = pos.x + dx[face.normal];
					int ny = pos.y + dy[face.normal];
					int nz = pos.z + dz[face.normal];

					size_t neighborIndex;
					const Chunk* neighborChunk = getChunkAndIndex_checkSideNeighbor(nx, ny, nz, face.normal, neighborIndex);
					if (!neighborChunk)
					{
						continue;
					}

					BlockId neighborBlock = neighborChunk->getBlockAt(neighborIndex);
					if (block == neighborBlock)
					{
						continue;
					}

					const BlockData* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
					if (neighborBlockData && neighborBlockData->faceCulling[face.normal ^ 1])
					{
						continue;
					}

					// Calculate shading
					LightLevel neighborLight = neighborChunk->getLightAt(neighborIndex);
					unsigned int ao, light;
					calculateFaceAmbientOcclusionAndLight(ao, light, pos.x, pos.y, pos.z, face.normal, neighborLight);

					// Get texture
					const auto& textureSlot = face.textureSlot < textureSlots.size() ? textureSlots[face.textureSlot] : fallbackTextureSlot;

					// Calculate texture transformation
					uint32_t faceTransformation = hash & transformationBitMasks[(size_t)textureSlot.transformation];

					// Add new face
					auto& instances = textureSlot.isTranslucent ? newInstances.alignedTranslucent[face.normal] : newInstances.alignedOpaque[face.normal];
					instances.emplace_back(
						pos.x, pos.y, pos.z,
						ao,
						textureSlot.textureId,
						faceTransformation,
						light
					);
				}
			}

			// Non-aligned faces
			// TODO: Non-aligned faces should be culled if they are on the block's border
			if (!model->nonAlignedFaces.empty())
			{
				BlockVertexLightData lightData;
				calculateBlockVertexLight(lightData, pos.x, pos.y, pos.z);

				NonAlignedBlockFace instance;
				instance.blockX = pos.x;
				instance.blockY = pos.y;
				instance.blockZ = pos.z;

				instance.light0 = lightData.light[0].fullByte;
				instance.light1 = lightData.light[1].fullByte;
				instance.light2 = lightData.light[2].fullByte;
				instance.light3 = lightData.light[3].fullByte;
				instance.light4 = lightData.light[4].fullByte;
				instance.light5 = lightData.light[5].fullByte;
				instance.light6 = lightData.light[6].fullByte;
				instance.light7 = lightData.light[7].fullByte;

				instance.ao0 = lightData.ao[0];
				instance.ao1 = lightData.ao[1];
				instance.ao2 = lightData.ao[2];
				instance.ao3 = lightData.ao[3];
				instance.ao4 = lightData.ao[4];
				instance.ao5 = lightData.ao[5];
				instance.ao6 = lightData.ao[6];
				instance.ao7 = lightData.ao[7];

				for (const auto& face : model->nonAlignedFaces)
				{
					const auto& textureSlot = face.textureSlot < textureSlots.size() ? textureSlots[face.textureSlot] : fallbackTextureSlot;

					instance.x0 = face.x0;
					instance.y0 = face.y0;
					instance.z0 = face.z0;

					instance.x1 = face.x1;
					instance.y1 = face.y1;
					instance.z1 = face.z1;

					instance.x2 = face.x2;
					instance.y2 = face.y2;
					instance.z2 = face.z2;

					instance.x3 = face.x3;
					instance.y3 = face.y3;
					instance.z3 = face.z3;

					instance.u0 = face.u0;
					instance.v0 = face.v0;

					instance.u1 = face.u1;
					instance.v1 = face.v1;

					instance.u2 = face.u2;
					instance.v2 = face.v2;

					instance.u3 = face.u3;
					instance.v3 = face.v3;

					instance.textureID = textureSlot.textureId;

					auto& instances = textureSlot.isTranslucent ? newInstances.nonAlignedTranslucent : newInstances.nonAlignedOpaque;

					instances.push_back(instance);
				}
			}
		}

		// Check if chunk got unloaded by the time we were building mesh
		if (!chunkFlags.read(Flag::IsLoadedInWorld))
		{
			return;
		}

		// Set mesh data
		ScopedProcessingFence scopedMeshFence(meshData.processingFence);

		meshData.instancesStorage = std::move(newInstances);

		size_t faceCount = meshData.getAllFaceCount();
		if (faceCount == 0)
		{
			meshData.dirty = false;

			// Update render face count if no changes (means no faces at all)
			meshData.updateRenderFaceCount();
		}
		else
		{
			meshData.dirty = true;
		}
	}
}

void Chunk::markMeshDirty()
{
	meshDirty = true;
}

void Chunk::askForMeshUpload()
{
	if (meshData.dirty)
	{
		pendingMeshUploads.push_back(&meshData);
	}
}

void Chunk::sendMeshesToGPU()
{
	if (pendingMeshUploads.empty())
	{
		return;
	}

	for (ChunkMeshData* chunkMesh : pendingMeshUploads)
	{
		chunkMesh->processingFence.startProcessing();
	}

	PROFILE_SCOPE("Send chunk meshes to GPU", ProfileCategory::ChunkMesh);

	// Allocate memory for meshes
	std::vector<ChunkMeshData*> allocateMemoryAlignedMeshRequests;
	allocateMemoryAlignedMeshRequests.reserve(pendingMeshUploads.size() >> 1);

	std::vector<ChunkMeshData*> allocateMemoryNonAlignedMeshRequests;
	allocateMemoryNonAlignedMeshRequests.reserve(pendingMeshUploads.size() >> 1);

	for (ChunkMeshData* chunkMesh : pendingMeshUploads)
	{
		if (chunkMesh->getAlignedFaceCount() > chunkMesh->getAlignedFaceCapacity())
		{
			allocateMemoryAlignedMeshRequests.push_back(chunkMesh);
		}
		if (chunkMesh->getNonAlignedFaceCount() > chunkMesh->getNonAlignedFaceCapacity())
		{
			allocateMemoryNonAlignedMeshRequests.push_back(chunkMesh);
		}
	}

	auto& chunkMeshManager = ChunkMeshManager::getInstance();
	chunkMeshManager.processMeshRequests(allocateMemoryAlignedMeshRequests, allocateMemoryNonAlignedMeshRequests);

	// Write meshes data
	auto& alignedInstancesVBO = chunkMeshManager.getAlignedInstanceVBO();
	auto& nonAlignedInstancesVBO = chunkMeshManager.getNonAlignedInstanceVBO();

	// Potential bug: If meshData.opaqueDirty and it will require more data then it was previously, then it will overwrite previous transparent data
	// But for now, both opaque and transparent parts are dirty, so this bug won't happen

	// Write aligned instances data
	for (ChunkMeshData* chunkMesh : pendingMeshUploads)
	{
		if (!chunkMesh->alignedCreated)
		{
			continue;
		}

		size_t opaqueFaceCount = chunkMesh->getAlignedOpaqueFaceCount();
		size_t translucentFaceCount = chunkMesh->getAlignedTranslucentFaceCount();

		if (opaqueFaceCount > 0)
		{
			//alignedInstancesVBO.write(
			//	chunkMesh->instancesStorage.alignedOpaque.data(),
			//	opaqueFaceCount * sizeof(AlignedBlockFace),
			//	chunkMesh->allocatedBlock_alignedFaces.offset * sizeof(AlignedBlockFace)
			//);

			size_t offset = chunkMesh->allocatedBlock_alignedFaces.offset;
			for (size_t i = 0; i < 6; i++)
			{
				size_t sideFaceCount = chunkMesh->instancesStorage.alignedOpaque[i].size();
				if (sideFaceCount == 0)
				{
					continue;
				}
				alignedInstancesVBO.write(
					chunkMesh->instancesStorage.alignedOpaque[i].data(),
					sideFaceCount * sizeof(AlignedBlockFace),
					offset * sizeof(AlignedBlockFace)
				);
				offset += sideFaceCount;
			}
		}

		if (translucentFaceCount > 0)
		{
			//alignedInstancesVBO.write(
			//	chunkMesh->instancesStorage.alignedTranslucent.data(),
			//	translucentFaceCount * sizeof(AlignedBlockFace),
			//	(chunkMesh->allocatedBlock_alignedFaces.offset + opaqueFaceCount) * sizeof(AlignedBlockFace)
			//);

			size_t offset = chunkMesh->allocatedBlock_alignedFaces.offset + opaqueFaceCount;
			for (size_t i = 0; i < 6; i++)
			{
				size_t sideFaceCount = chunkMesh->instancesStorage.alignedTranslucent[i].size();
				if (sideFaceCount == 0)
				{
					continue;
				}
				alignedInstancesVBO.write(
					chunkMesh->instancesStorage.alignedTranslucent[i].data(),
					sideFaceCount * sizeof(AlignedBlockFace),
					offset * sizeof(AlignedBlockFace)
				);
				offset += sideFaceCount;
			}
		}
	}

	// Write non-aligned instances data
	for (ChunkMeshData* chunkMesh : pendingMeshUploads)
	{
		if (!chunkMesh->nonAlignedCreated)
		{
			continue;
		}

		size_t opaqueFaceCount = chunkMesh->getNonAlignedOpaqueFaceCount();
		size_t translucentFaceCount = chunkMesh->getNonAlignedTranslucentFaceCount();

		if (opaqueFaceCount > 0)
		{
			nonAlignedInstancesVBO.write(
				chunkMesh->instancesStorage.nonAlignedOpaque.data(),
				opaqueFaceCount * sizeof(NonAlignedBlockFace),
				chunkMesh->allocatedBlock_nonAlignedFaces.offset * sizeof(NonAlignedBlockFace)
			);
		}

		if (translucentFaceCount > 0)
		{
			nonAlignedInstancesVBO.write(
				chunkMesh->instancesStorage.nonAlignedTranslucent.data(),
				translucentFaceCount * sizeof(NonAlignedBlockFace),
				(chunkMesh->allocatedBlock_nonAlignedFaces.offset + opaqueFaceCount) * sizeof(NonAlignedBlockFace)
			);
		}
	}

	//
	for (ChunkMeshData* chunkMesh : pendingMeshUploads)
	{
		chunkMesh->updateRenderFaceCount();
		chunkMesh->dirty = false;
		//chunkMesh->clearInstances(); // Can be cleared, but it won't change anything.
		chunkMesh->processingFence.stopProcessing();
	}
	pendingMeshUploads.clear();
}

void Chunk::updateEnabledMeshSides(const glm::ivec3& cameraChunkPosition)
{
	enabledAlignedOpaqueFaceSides[0] = cameraChunkPosition.x <= position.x;
	enabledAlignedOpaqueFaceSides[1] = cameraChunkPosition.x >= position.x;
	enabledAlignedOpaqueFaceSides[2] = cameraChunkPosition.y <= position.y;
	enabledAlignedOpaqueFaceSides[3] = cameraChunkPosition.y >= position.y;
	enabledAlignedOpaqueFaceSides[4] = cameraChunkPosition.z <= position.z;
	enabledAlignedOpaqueFaceSides[5] = cameraChunkPosition.z >= position.z;
}

void Chunk::collectAlignedOpaqueRenderData(
	std::vector<DrawArraysIndirectCommand>& drawCommands,
	std::vector<glm::ivec3>& positions,
	std::vector<uint32_t>& positionIndices,
	ChunkNormalPacker& normalPacker
) const
{
	if (!meshData.alignedCreated)
	{
		return;
	}

	const auto& enabledSides = enabledAlignedOpaqueFaceSides;

	size_t offset = meshData.allocatedBlock_alignedFaces.offset;

	bool anySide = false;
	for (int side = 0; side < 6; side++)
	{
		size_t faceCount = meshData.renderAlignedOpaqueFaceCount[side];
		if (faceCount == 0)
		{
			continue;
		}

		if (enabledSides[side])
		{
			drawCommands.emplace_back(4, faceCount, 0, offset);
			positionIndices.push_back(positions.size());
			normalPacker.addNormal(side);
			anySide = true;
		}
		offset += faceCount;
	}
	if (anySide)
	{
		positions.push_back(position);
	}
}

void Chunk::collectAlignedTranslucentRenderData(
	std::vector<DrawArraysIndirectCommand>& drawCommands,
	std::vector<glm::ivec3>& positions,
	std::vector<uint32_t>& positionIndices,
	ChunkNormalPacker& normalPacker
) const
{
	if (!meshData.alignedCreated)
	{
		return;
	}

	size_t offset = meshData.allocatedBlock_alignedFaces.offset + meshData.getAlignedOpaqueFaceCount();

	bool anySide = false;
	for (int side = 0; side < 6; side++)
	{
		size_t faceCount = meshData.renderAlignedTranslucentFaceCount[side];
		if (faceCount == 0)
		{
			continue;
		}

		drawCommands.emplace_back(4, faceCount, 0, offset);
		positionIndices.push_back(positions.size());
		normalPacker.addNormal(side);
		anySide = true;

		offset += faceCount;
	}
	if (anySide)
	{
		positions.push_back(position);
	}
}

void Chunk::collectNonAlignedOpaqueRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const
{
	size_t faceCount = meshData.renderNonAlignedOpaqueFaceCount;
	if (!meshData.nonAlignedCreated || faceCount == 0)
	{
		return;
	}
	drawCommands.emplace_back(4, faceCount, 0, meshData.allocatedBlock_nonAlignedFaces.offset);
	positions.push_back(position);
}

void Chunk::collectNonAlignedTranslucentRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const
{
	size_t faceCount = meshData.renderNonAlignedTranslucentFaceCount;
	if (!meshData.nonAlignedCreated || faceCount == 0)
	{
		return;
	}
	drawCommands.emplace_back(4, faceCount, 0, meshData.allocatedBlock_nonAlignedFaces.offset + meshData.renderNonAlignedOpaqueFaceCount);
	positions.push_back(position);
}

const Chunk* Chunk::getChunkAndIndex_checkSideNeighbor(int x, int y, int z, int side, size_t& outIndex) const
{
	int check = (x | y | z) & CHUNK_UPPER_BITS_MASK;
	if (check == 0)
	{
		outIndex = getIndex(x, y, z);
		return this;
	}

	const Chunk* neighbor = neighbors[side];
	if (neighbor)
	{
		outIndex = getIndex(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
		return neighbor;
	}

	return nullptr;
}

Chunk* Chunk::getChunkAndIndex_checkSideNeighbor(int x, int y, int z, int side, size_t& outIndex)
{
	return const_cast<Chunk*>(const_cast<const Chunk*>(this)->getChunkAndIndex_checkSideNeighbor(x, y, z, side, outIndex));
}

const Chunk* Chunk::getChunkAndIndex_checkNeighborsTraverse(int x, int y, int z, size_t& outIndex) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx == 0 && ny == 0 && nz == 0)
	{
		outIndex = getIndex(x, y, z);
		return this;
	}

	int dirX = (nx < 0) ? 0 : ((nx > 0) ? 1 : -1);
	int dirY = (ny < 0) ? 2 : ((ny > 0) ? 3 : -1);
	int dirZ = (nz < 0) ? 4 : ((nz > 0) ? 5 : -1);

	const Chunk* neighbor = this;

	if (dirX != -1)
	{
		neighbor = neighbor->neighbors[dirX];
		if (!neighbor) return nullptr;
	}

	if (dirY != -1)
	{
		neighbor = neighbor->neighbors[dirY];
		if (!neighbor) return nullptr;
	}

	if (dirZ != -1)
	{
		neighbor = neighbor->neighbors[dirZ];
		if (!neighbor) return nullptr;
	}

	outIndex = getIndex(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
	return neighbor;
}

Chunk* Chunk::getChunkAndIndex_checkNeighborsTraverse(int x, int y, int z, size_t& outIndex)
{
	return const_cast<Chunk*>(const_cast<const Chunk*>(this)->getChunkAndIndex_checkNeighborsTraverse(x, y, z, outIndex));
}

BlockId Chunk::getBlockAt(int x, int y, int z) const
{
	return blocks[getIndex(x, y, z)];
}

LightLevel Chunk::getLightAt(int x, int y, int z) const
{
	return lightLevels[getIndex(x, y, z)];
}

std::pair<BlockId, LightLevel> Chunk::getBlockAndLightAt(int x, int y, int z) const
{
	size_t index = getIndex(x, y, z);
	return std::make_pair(blocks[index], lightLevels[index]);
}

BlockId Chunk::getBlockAt(size_t index) const
{
	return blocks[index];
}

LightLevel Chunk::getLightAt(size_t index) const
{
	return lightLevels[index];
}

std::pair<BlockId, LightLevel> Chunk::getBlockAndLightAt(size_t index) const
{
	return std::make_pair(blocks[index], lightLevels[index]);
}

void Chunk::setBlockAt(int x, int y, int z, BlockId block, bool saveBlockChanges)
{
	size_t index = getIndex(x, y, z);

	BlockId previousBlock = blocks[index];
	if (previousBlock == block)
	{
		return;
	}

	// Update array
	blocks[index] = block;

	// Update changedBlocks map
	if (saveBlockChanges)
	{
		removeIndexFromMap(previousBlock, static_cast<uint16_t>(index));
		changedBlocks[block].push_back(static_cast<uint16_t>(index));
	}

	// Light update
	const BlockData* previousBlockData = AssetRegistry::getBlockDataSafe(previousBlock);
	uint8_t previousEmission = previousBlockData->lightEmission;

	const BlockData* newBlockData = AssetRegistry::getBlockDataSafe(block);
	uint8_t newEmission = newBlockData->lightEmission;

	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };
	if (previousBlockData->absorbsLight && !newBlockData->absorbsLight)
	{
		// Collect maximum light level from neighbors and propagate it to this block
		uint8_t maxBlockLightToSet = 0;
		uint8_t maxSkyLightToSet = 0;
		for (int i = 0; i < 6; i++)
		{
			int nx = x + dx[i];
			int ny = y + dy[i];
			int nz = z + dz[i];

			size_t neighborIndex;
			Chunk* neighborChunk = getChunkAndIndex_checkSideNeighbor(nx, ny, nz, i, neighborIndex);
			if (!neighborChunk)
			{
				continue;
			}

			LightLevel neighborLight = neighborChunk->getLightAt(neighborIndex);

			if (neighborLight.blockLight > maxBlockLightToSet && neighborLight.blockLight > 1)
			{
				maxBlockLightToSet = neighborLight.blockLight - 1;
			}

			uint8_t skyLightAbsorption = (i == 3 && neighborLight.skyLight == 15) ? 0 : 1;
			if (neighborLight.skyLight > maxSkyLightToSet && neighborLight.skyLight > skyLightAbsorption)
			{
				maxSkyLightToSet = neighborLight.skyLight - skyLightAbsorption;
			}
		}

		if (maxBlockLightToSet > 0)
		{
			lightLevels[index].blockLight = maxBlockLightToSet;
			if (maxBlockLightToSet > 1) addBlockLightNodeToQueue(x, y, z);
		}
		if (maxSkyLightToSet > 0)
		{
			lightLevels[index].skyLight = maxSkyLightToSet;
			if (maxSkyLightToSet > 1) addSkyLightNodeToQueue(x, y, z);
		}
	}
	else if (!previousBlockData->absorbsLight && newBlockData->absorbsLight)
	{
		// Remove light at this block
		uint8_t currentBlockLight = lightLevels[index].blockLight;
		if (currentBlockLight > 0)
		{
			lightLevels[index].blockLight = 0;
			addBlockLightRemovalNodeToQueue(x, y, z, currentBlockLight);
		}
		uint8_t currentSkyLight = lightLevels[index].skyLight;
		if (currentSkyLight > 0)
		{
			lightLevels[index].skyLight = 0;
			addSkyLightRemovalNodeToQueue(x, y, z, currentSkyLight);
		}
	}

	// Handle light emission changes
	if (previousEmission != newEmission)
	{
		if (previousEmission > newEmission)
		{
			lightLevels[index].blockLight = 0;
			addBlockLightRemovalNodeToQueue(x, y, z, previousEmission);
		}

		if (newEmission > 0)
		{
			lightLevels[index].blockLight = newEmission;
			addBlockLightNodeToQueue(x, y, z);
		}
	}

	// Mark meshes as dirty
	markBlockMeshDirty(x, y, z);
}

void Chunk::setLightAt(int x, int y, int z, LightLevel lightValue)
{
	lightLevels[getIndex(x, y, z)] = lightValue;
	markBlockMeshDirty(x, y, z);
}

void Chunk::setBlockLightAt(int x, int y, int z, uint8_t lightLevel)
{
	lightLevels[getIndex(x, y, z)].blockLight = lightLevel;
	markBlockMeshDirty(x, y, z);
}

void Chunk::setSkyLightAt(int x, int y, int z, uint8_t lightLevel)
{
	lightLevels[getIndex(x, y, z)].skyLight = lightLevel;
	markBlockMeshDirty(x, y, z);
}

void Chunk::setLightAt(size_t index, LightLevel lightValue)
{
	lightLevels[index] = lightValue;

	glm::ivec3 pos = getPositionFromIndex(index);
	markBlockMeshDirty(pos.x, pos.y, pos.z);
}

void Chunk::setBlockLightAt(size_t index, uint8_t lightLevel)
{
	lightLevels[index].blockLight = lightLevel;

	glm::ivec3 pos = getPositionFromIndex(index);
	markBlockMeshDirty(pos.x, pos.y, pos.z);
}

void Chunk::setSkyLightAt(size_t index, uint8_t lightLevel)
{
	lightLevels[index].skyLight = lightLevel;

	glm::ivec3 pos = getPositionFromIndex(index);
	markBlockMeshDirty(pos.x, pos.y, pos.z);
}

void Chunk::addBlockLightNodeToQueue(int x, int y, int z)
{
	std::lock_guard<std::mutex> lock(blockLightBfsMutex);
	blockLightBfsQueue.emplace(x, y, z);
}

void Chunk::addBlockLightRemovalNodeToQueue(int x, int y, int z, uint8_t lightLevel)
{
	std::lock_guard<std::mutex> lock(blockLightRemovalBfsMutex);
	blockLightRemovalBfsQueue.emplace(x, y, z, lightLevel);
}

void Chunk::addSkyLightNodeToQueue(int x, int y, int z)
{
	std::lock_guard<std::mutex> lock(skyLightBfsMutex);
	skyLightBfsQueue.emplace(x, y, z);
}

void Chunk::addSkyLightRemovalNodeToQueue(int x, int y, int z, uint8_t lightLevel)
{
	std::lock_guard<std::mutex> lock(skyLightRemovalBfsMutex);
	skyLightRemovalBfsQueue.emplace(x, y, z, lightLevel);
}

void Chunk::markBlockMeshDirty(int x, int y, int z)
{
	meshDirty = true;

	// Mark neighbor meshes as dirty
	const bool left = x == 0;
	const bool right = x == (CHUNK_SIZE - 1);
	const bool bottom = y == 0;
	const bool top = y == (CHUNK_SIZE - 1);
	const bool back = z == 0;
	const bool front = z == (CHUNK_SIZE - 1);

	// Early exit if not on any boundary
	if (!(left || right || bottom || top || back || front)) return;

	// Sides
	Chunk* n0;
	if (left   && (n0 = neighbors[0])) n0->meshDirty = true;
	if (right  && (n0 = neighbors[1])) n0->meshDirty = true;
	if (bottom && (n0 = neighbors[2])) n0->meshDirty = true;
	if (top    && (n0 = neighbors[3])) n0->meshDirty = true;
	if (back   && (n0 = neighbors[4])) n0->meshDirty = true;
	if (front  && (n0 = neighbors[5])) n0->meshDirty = true;

	// Edges
	Chunk* n1;
	if (left   && bottom && (n0 = neighbors[0]) && (n1 = n0->neighbors[2])) n1->meshDirty = true;
	if (left   && top    && (n0 = neighbors[0]) && (n1 = n0->neighbors[3])) n1->meshDirty = true;
	if (right  && bottom && (n0 = neighbors[1]) && (n1 = n0->neighbors[2])) n1->meshDirty = true;
	if (right  && top    && (n0 = neighbors[1]) && (n1 = n0->neighbors[3])) n1->meshDirty = true;
	if (left   && back   && (n0 = neighbors[0]) && (n1 = n0->neighbors[4])) n1->meshDirty = true;
	if (left   && front  && (n0 = neighbors[0]) && (n1 = n0->neighbors[5])) n1->meshDirty = true;
	if (right  && back   && (n0 = neighbors[1]) && (n1 = n0->neighbors[4])) n1->meshDirty = true;
	if (right  && front  && (n0 = neighbors[1]) && (n1 = n0->neighbors[5])) n1->meshDirty = true;
	if (bottom && back   && (n0 = neighbors[2]) && (n1 = n0->neighbors[4])) n1->meshDirty = true;
	if (bottom && front  && (n0 = neighbors[2]) && (n1 = n0->neighbors[5])) n1->meshDirty = true;
	if (top    && back   && (n0 = neighbors[3]) && (n1 = n0->neighbors[4])) n1->meshDirty = true;
	if (top    && front  && (n0 = neighbors[3]) && (n1 = n0->neighbors[5])) n1->meshDirty = true;

	// Corners
	Chunk* n2;
	if (left && bottom && back &&
		(n0 = neighbors[0]) &&
		(n1 = n0->neighbors[2]) &&
		(n2 = n1->neighbors[4]))
	{
		n2->meshDirty = true;
	}
	if (left && bottom && front &&
		(n0 = neighbors[0]) &&
		(n1 = n0->neighbors[2]) &&
		(n2 = n1->neighbors[5]))
	{
		n2->meshDirty = true;
	}
	if (left && top && back &&
		(n0 = neighbors[0]) &&
		(n1 = n0->neighbors[3]) &&
		(n2 = n1->neighbors[4]))
	{
		n2->meshDirty = true;
	}
	if (left && top && front &&
		(n0 = neighbors[0]) &&
		(n1 = n0->neighbors[3]) &&
		(n2 = n1->neighbors[5]))
	{
		n2->meshDirty = true;
	}
	if (right && bottom && back &&
		(n0 = neighbors[1]) &&
		(n1 = n0->neighbors[2]) &&
		(n2 = n1->neighbors[4]))
	{
		n2->meshDirty = true;
	}
	if (right && bottom && front &&
		(n0 = neighbors[1]) &&
		(n1 = n0->neighbors[2]) &&
		(n2 = n1->neighbors[5]))
	{
		n2->meshDirty = true;
	}
	if (right && top && back &&
		(n0 = neighbors[1]) &&
		(n1 = n0->neighbors[3]) &&
		(n2 = n1->neighbors[4]))
	{
		n2->meshDirty = true;
	}
	if (right && top && front &&
		(n0 = neighbors[1]) &&
		(n1 = n0->neighbors[3]) &&
		(n2 = n1->neighbors[5]))
	{
		n2->meshDirty = true;
	}
}

void Chunk::calculateVertexAmbientOcclusionAndLight(
	unsigned int& ao,
	LightLevel& light,
	LightLevel centerLight,
	const std::pair<LightLevel, bool>& side1,
	const std::pair<LightLevel, bool>& side2,
	const std::pair<LightLevel, bool>& corner
) const
{
	unsigned int blockLightSum = centerLight.blockLight;
	unsigned int skyLightSum = centerLight.skyLight;
	unsigned int count = 1;

	if (side1.second && side2.second)
	{
		ao = 0;
		light = centerLight;
		return;
	}

	if (!side1.second)
	{
		blockLightSum += side1.first.blockLight;
		skyLightSum += side1.first.skyLight;
		count++;
	}

	if (!side2.second)
	{
		blockLightSum += side2.first.blockLight;
		skyLightSum += side2.first.skyLight;
		count++;
	}

	if (!corner.second)
	{
		blockLightSum += corner.first.blockLight;
		skyLightSum += corner.first.skyLight;
		count++;
	}

	unsigned int avgBlockLight = blockLightSum / count;
	unsigned int avgSkyLight = skyLightSum / count;

	ao = 3 - (side1.second + side2.second + corner.second);
	light.blockLight = avgBlockLight;
	light.skyLight = avgSkyLight;
}

void Chunk::calculateFaceAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, int x, int y, int z, int normal, LightLevel centerFaceLight) const
{
	// For each face normal, we need to check 8 neighbors around the face
	// The AO calculation depends on which direction the face is facing

	std::pair<LightLevel, bool> neighborData[8];

	unsigned int ao0, ao1, ao2, ao3;
	LightLevel lightLevels[4];

	auto getSafe = [this, &neighborData, normal](size_t dataIdx, int x_, int y_, int z_)
		{
			size_t idx;
			const Chunk* c = getChunkAndIndex_checkNeighborsTraverse(x_, y_, z_, idx);

			neighborData[dataIdx].second = true;
			if (c)
			{
				auto data = c->getBlockAndLightAt(idx);
				neighborData[dataIdx].first = data.second;
				const auto* blockData = AssetRegistry::getBlockData(data.first);
				neighborData[dataIdx].second = blockData && blockData->faceCulling[normal ^ 1];
			}
		};

	switch (normal)
	{
	case 0: // -X face

		getSafe(0, x - 1, y - 1, z - 1);
		getSafe(1, x - 1, y - 1, z);
		getSafe(2, x - 1, y - 1, z + 1);
		getSafe(3, x - 1, y, z - 1);
		getSafe(4, x - 1, y, z + 1);
		getSafe(5, x - 1, y + 1, z - 1);
		getSafe(6, x - 1, y + 1, z);
		getSafe(7, x - 1, y + 1, z + 1);

		// TODO: Data can be a pair of light level and solid boolean
		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		break;
	case 1: // +X face
		getSafe(0, x + 1, y - 1, z - 1);
		getSafe(1, x + 1, y - 1, z);
		getSafe(2, x + 1, y - 1, z + 1);
		getSafe(3, x + 1, y, z - 1);
		getSafe(4, x + 1, y, z + 1);
		getSafe(5, x + 1, y + 1, z - 1);
		getSafe(6, x + 1, y + 1, z);
		getSafe(7, x + 1, y + 1, z + 1);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 2: // -Y face
		getSafe(0, x - 1, y - 1, z - 1);
		getSafe(1, x, y - 1, z - 1);
		getSafe(2, x + 1, y - 1, z - 1);
		getSafe(3, x - 1, y - 1, z);
		getSafe(4, x + 1, y - 1, z);
		getSafe(5, x - 1, y - 1, z + 1);
		getSafe(6, x, y - 1, z + 1);
		getSafe(7, x + 1, y - 1, z + 1);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 3: // +Y face
		getSafe(0, x - 1, y + 1, z - 1);
		getSafe(1, x, y + 1, z - 1);
		getSafe(2, x + 1, y + 1, z - 1);
		getSafe(3, x - 1, y + 1, z);
		getSafe(4, x + 1, y + 1, z);
		getSafe(5, x - 1, y + 1, z + 1);
		getSafe(6, x, y + 1, z + 1);
		getSafe(7, x + 1, y + 1, z + 1);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[4], neighborData[1], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[3], neighborData[1], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[3], neighborData[6], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[4], neighborData[6], neighborData[7]);
		break;
	case 4: // -Z face
		getSafe(0, x - 1, y - 1, z - 1);
		getSafe(1, x, y - 1, z - 1);
		getSafe(2, x + 1, y - 1, z - 1);
		getSafe(3, x - 1, y, z - 1);
		getSafe(4, x + 1, y, z - 1);
		getSafe(5, x - 1, y + 1, z - 1);
		getSafe(6, x, y + 1, z - 1);
		getSafe(7, x + 1, y + 1, z - 1);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 5: // +Z face
		getSafe(0, x - 1, y - 1, z + 1);
		getSafe(1, x, y - 1, z + 1);
		getSafe(2, x + 1, y - 1, z + 1);
		getSafe(3, x - 1, y, z + 1);
		getSafe(4, x + 1, y, z + 1);
		getSafe(5, x - 1, y + 1, z + 1);
		getSafe(6, x, y + 1, z + 1);
		getSafe(7, x + 1, y + 1, z + 1);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		break;
	}

	//
	ao = ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6);
	light = *((unsigned int*)lightLevels);
}

void Chunk::calculateBlockVertexLight(BlockVertexLightData& result, int x, int y, int z) const
{
	for (int i = 0; i < 8; i++)
	{
		result.light[i].fullByte = 255u;
		result.ao[i] = 3u;
	}
	return;
	//LightLevel lightData[27];
	//bool solidNeighbors[27];

	//constexpr auto getIndex3x3 = [](int dx, int dy, int dz) constexpr {
	//	return (dx + 1) * 9 + (dy + 1) * 3 + (dz + 1);
	//	};

	//auto getSafe = [this, &lightData, &solidNeighbors](size_t dataIdx, int x_, int y_, int z_)
	//	{
	//		size_t idx;
	//		const Chunk* c = getChunkAndIndex_checkNeighborsTraverse(x_, y_, z_, idx);

	//		solidNeighbors[dataIdx] = true;
	//		if (c)
	//		{
	//			auto data = c->getBlockAndLightAt(idx);
	//			lightData[dataIdx] = data.second;
	//			solidNeighbors[dataIdx] = !BlockRegistry::getBlockDataByID(data.first)->properties.areFacesTranslucent;
	//		}
	//	};

	//size_t idx = 0;
	//for (int dx = -1; dx <= 1; dx++)
	//{
	//	for (int dy = -1; dy <= 1; dy++)
	//	{
	//		for (int dz = -1; dz <= 1; dz++)
	//		{
	//			getSafe(idx++, x + dx, y + dy, z + dz);
	//		}
	//	}
	//}
	//// TODO: Maybe center always should be not solid?

	//// -1 -1 -1
	//{
	//	const int indices[8] = {
	//		getIndex3x3( 0, -1,  0),
	//		getIndex3x3(-1, -1,  0),
	//		getIndex3x3( 0, -1, -1),
	//		getIndex3x3(-1, -1, -1),
	//		getIndex3x3( 0,  0,  0),
	//		getIndex3x3(-1,  0,  0),
	//		getIndex3x3( 0,  0, -1),
	//		getIndex3x3(-1,  0, -1),
	//	};

	//	unsigned int blockLightSum = 0;
	//	unsigned int skyLightSum = 0;
	//	unsigned int validSamplesCount = 0;
	//	for (int index : indices)
	//	{
	//		if (solidNeighbors[index])
	//		{
	//			continue;
	//		}

	//		blockLightSum += lightData[index].blockLight;
	//		skyLightSum += lightData[index].skyLight;
	//		validSamplesCount++;
	//	}

	//	unsigned int avgBlockLight = 0;
	//	unsigned int avgSkyLight = 0;
	//	if (validSamplesCount > 0)
	//	{
	//		avgBlockLight = blockLightSum / validSamplesCount;
	//		avgSkyLight = skyLightSum / validSamplesCount;
	//	}

	//	result.light[0].blockLight = avgBlockLight;
	//	result.light[0].skyLight = avgSkyLight;
	//	result.ao[0] = (float)validSamplesCount / 8.0f * 3.0f;
	//}

	////result.light[0] = LightLevel(15, 15);
	////result.ao[0] = 3;
}

void Chunk::setState(State newState)
{
	state.store(newState, std::memory_order_release);
}

void Chunk::addLoader()
{
	loaderCount++;
}

void Chunk::removeLoader()
{
	loaderCount--;
}

//============================================================================
//LightLevel

LightLevel::LightLevel() :
	fullByte(0)
{
}

LightLevel::LightLevel(uint8_t blockLight, uint8_t skyLight) :
	blockLight(blockLight), skyLight(skyLight)
{
}

LightLevel::LightLevel(const LightLevel& other) :
	fullByte(other.fullByte)
{
}

LightLevel& LightLevel::operator=(const LightLevel& other)
{
	fullByte = other.fullByte;
	return *this;
}

//============================================================================
//LightNode

LightNode::LightNode(int x, int y, int z) :
	x(x), y(y), z(z)
{
}

//============================================================================
//LightNode

LightRemovalNode::LightRemovalNode(int x, int y, int z, uint8_t lightLevel) :
	x(x), y(y), z(z), lightLevel(lightLevel)
{
}

//============================================================================
//DrawArraysIndirectCommand

DrawArraysIndirectCommand::DrawArraysIndirectCommand(unsigned int count, unsigned int instanceCount, unsigned int first, unsigned int baseInstance) :
	count(count), instanceCount(instanceCount), first(first), baseInstance(baseInstance)
{
}

//============================================================================