#include "Chunk.h"
#include "Chunk/TerrainGenerator.h"
#include "Chunk/ChunkMeshManager.h"
#include "Chunk/BlockData.h"

#include "Core/Profiler.h"
#include "Core/SymmetricBitMatrix.h"
#include "Core/ASSERT.h"
#include "Core/Hashes/ivec2Hasher.h"

#include <vector>
#include <format>
#include <fstream>

#define CHUNK_SMOOTH_LIGHTING 1

std::vector<MeshData*> Chunk::pendingMeshUploads;
StructureBlockChangeManager Chunk::structureBlockChangeManager;
std::filesystem::path Chunk::WORLD_PATH;

inline size_t Chunk::getIndex(int x, int y, int z)
{
	return (x << (CHUNK_SIZE_LOG2 << 1)) | (y << CHUNK_SIZE_LOG2) | z;
}

glm::ivec3 Chunk::getPositionFromIndex(size_t index)
{
	return {
		(index >> (CHUNK_SIZE_LOG2 << 1)) & CHUNK_LOWER_BITS_MASK,
		(index >> CHUNK_SIZE_LOG2) & CHUNK_LOWER_BITS_MASK,
		index & CHUNK_LOWER_BITS_MASK
	};
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

	meshData.resetFaceCount();
	meshData.opaqueDirty = false;
	meshData.transparentDirty = false;

	meshDirty = false;

	cameraClosestBlockPosForSortingMesh = -1;
	shouldSortMeshAfterBuild = false;
	
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
	meshData.opaqueInstances.clear();
	meshData.transparentInstances.clear();

	// Release chunk column data
	if (chunkFlags.read(Flag::IsLoadedChunkColumnData))
	{
		chunkFlags.set(Flag::IsLoadedChunkColumnData, false);
		TerrainGenerator::getInstance().unloadChunkColumnData(position.x, position.z);
	}

	// Clear light BFS queues
	{
		std::lock_guard<std::mutex> lock(blockLightBfsMutex);
		while (!blockLightBfsQueue.empty())
		{
			blockLightBfsQueue.pop();
		}
	}
	{
		std::lock_guard<std::mutex> lock(blockLightRemovalBfsMutex);
		while (!blockLightRemovalBfsQueue.empty())
		{
			blockLightRemovalBfsQueue.pop();
		}
	}
	{
		std::lock_guard<std::mutex> lock(skyLightBfsMutex);
		while (!skyLightBfsQueue.empty())
		{
			skyLightBfsQueue.pop();
		}
	}
	{
		std::lock_guard<std::mutex> lock(skyLightRemovalBfsMutex);
		while (!skyLightRemovalBfsQueue.empty())
		{
			skyLightRemovalBfsQueue.pop();
		}
	}

	// TODO: Make it async. Mark chunk as processing.
	saveBlocks();
	changedBlocks.clear();
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
	const ChunkColumnData* chunkColumnData = TerrainGenerator::getInstance().loadChunkColumnData(position.x, position.z);
	chunkFlags.set(Flag::IsLoadedChunkColumnData, true);
	const int* heightMap = chunkColumnData->heightMapRead();

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

					Block block = Block::Air;
					if (globalY > globalHeight)
					{
						block = ocean ? Block::Water : Block::Air;
					}
					else if (globalY == globalHeight)
					{
						block = Block::GrassBlock;
					}
					else if (globalY > globalHeight - 4)
					{
						block = Block::Dirt;
					}
					else
					{
						block = Block::Stone;
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
			if (blocks[i] == Block::Stone && caveMask[i])
			{
				blocks[i] = Block::Air;
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
				if (blocks[rootIndex] != Block::Air)
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
			if (!change.placeIfBlockIsAir || blocks[change.index] == Block::Air)
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

bool Chunk::hasStructureBlockUpdates() const
{
	return structureBlockChangeManager.hasPendingChanges(position);
}

void Chunk::updateStructureBlocks()
{
	ScopedProcessingFence scopedFence(processingFence);

	auto pendingChanges = structureBlockChangeManager.retrieveAndClearChanges(position);
	for (const auto& change : pendingChanges)
	{
		if (!change.placeIfBlockIsAir || blocks[change.index] == Block::Air)
		{
			auto pos = getPositionFromIndex(change.index);
			setBlockAt(pos.x, pos.y, pos.z, change.block, false);
		}
	}
}

void Chunk::generateTree(const glm::ivec3& rootPosition)
{
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
		blocks[index] = Block::LogOak;
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
					if (blocks[index] == Block::Air)
					{
						blocks[index] = Block::LeavesOak;
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

					structureBlockChangeManager.addChange(chunkPos, Block::LeavesOak, index, true);
				}
			}
		}
	}
}

void Chunk::loadBlocks()
{
	ASSERT(changedBlocks.empty());
	PROFILE_SCOPE("Load chunk blocks", ProfileCategory::ChunkBlocks);

	namespace fs = std::filesystem;
	std::string name = std::format("{}_{}_{}.bin", position.x, position.y, position.z);
	fs::path chunkPath = WORLD_PATH / "Chunks" / name;

	if (!fs::exists(chunkPath))
	{
		return;
	}

	// TODO: Maybe add exceptions

	std::ifstream in(chunkPath, std::ios::binary);
	if (!in) return;

	// Load data from file
	uint16_t mapSize = 0;
	in.read(reinterpret_cast<char*>(&mapSize), sizeof(mapSize));

	for (uint16_t i = 0; i < mapSize; i++)
	{
		Block block;
		uint16_t count;
		in.read(reinterpret_cast<char*>(&block), sizeof(block));
		in.read(reinterpret_cast<char*>(&count), sizeof(count));

		std::vector<uint16_t> indices(count);
		in.read(reinterpret_cast<char*>(indices.data()), count * sizeof(uint16_t));

		changedBlocks[block] = std::move(indices);
	}

	// Apply loaded data
	// Remove unnecessary changes
	for (auto it = changedBlocks.begin(); it != changedBlocks.end();)
	{
		Block block = it->first;
		auto& indices = it->second;

		// We’ll remove redundant ones during iteration
		size_t writeIndex = 0;

		for (uint16_t index : indices)
		{
			if (blocks[index] == block) 
			{
				continue;
			}

			// Apply the change
			blocks[index] = block;
			indices[writeIndex++] = index; // keep this index as still changed
		}

		if (writeIndex == 0)
		{
			// no valid indices left, erase this block type entirely
			it = changedBlocks.erase(it);
		}
		else
		{
			indices.resize(writeIndex);
			++it;
		}
	}
}

void Chunk::saveBlocks() const
{
	if (changedBlocks.empty())
	{
		return;
	}

	// TODO: If changedBlocks is empty and files exists, maybe delete file?

	PROFILE_SCOPE("Save chunk blocks", ProfileCategory::ChunkBlocks);

	namespace fs = std::filesystem;
	std::string name = std::format("{}_{}_{}.bin", position.x, position.y, position.z);
	fs::path chunkPath = WORLD_PATH / "Chunks" / name;

	std::ofstream out(chunkPath, std::ios::binary);
	uint16_t mapSize = static_cast<uint16_t>(changedBlocks.size());
	out.write(reinterpret_cast<const char*>(&mapSize), sizeof(mapSize));

	for (const auto& [block, indices] : changedBlocks)
	{
		uint16_t count = static_cast<uint16_t>(indices.size());
		out.write(reinterpret_cast<const char*>(&block), sizeof(block));
		out.write(reinterpret_cast<const char*>(&count), sizeof(count));
		out.write(reinterpret_cast<const char*>(indices.data()), count * sizeof(uint16_t));
	}
}

bool Chunk::findFloodFillStartIndex(uint16_t& startIndex, const bool* floodFillMask) const
{
	for (uint16_t i = startIndex; i < CHUNK_VOLUME; i++)
	{
		if (floodFillMask[i])
		{
			// Already visited
			continue;
		}

		Block block = blocks[i];
		const BlockData* blockData = BlockDataBase::getBlockData(block);
		if (!blockData->properties.areFacesTransparent)
		{
			// Block isn't transparent
			continue;
		}

		startIndex = i;
		return true;
	}
	return false;
}

void Chunk::computeConnectivity()
{
	PROFILE_SCOPE("Compute chunk connectivity", ProfileCategory::General);

	constexpr glm::ivec3 dirs[6] =
	{
		{-1, 0, 0}, {1, 0, 0},
		{0, -1, 0}, {0, 1, 0},
		{0, 0, -1}, {0, 0, 1}
	};

	// Reset regions
	bool visitedCells[CHUNK_VOLUME]; // TODO: Can be a bitset
	std::fill(visitedCells, visitedCells + CHUNK_VOLUME, false);

	SymmetricBitMatrix<6> chunkConnectivity; // 6x6 matrix
	chunkConnectivity.fill(false);

	uint16_t startIndex = 0;

	std::vector<glm::ivec3> cellsToVisit;

	//static std::mutex mtx;
	//std::lock_guard<std::mutex> lock(mtx);

	while (true)
	{
		// Find start index
		if (!findFloodFillStartIndex(startIndex, visitedCells))
		{
			break;
		}

		glm::ivec3 startPos = getPositionFromIndex(startIndex);
		visitedCells[startIndex] = true; // Mark as visited
		startIndex++; // Increment, so 'findFloodFillStartIndex' will look immediately at next block

		bool regionConnectivity[6] = { false, false, false, false, false, false }; // TODO: Can be a bitset

		cellsToVisit.push_back(startPos);
		while (!cellsToVisit.empty())
		{
			// Get cell
			glm::ivec3 cell = cellsToVisit.back();
			cellsToVisit.pop_back();

			// Check if cell is on chunk border
			regionConnectivity[0] |= cell.x == 0;
			regionConnectivity[1] |= cell.x == (CHUNK_SIZE - 1);
			regionConnectivity[2] |= cell.y == 0;
			regionConnectivity[3] |= cell.y == (CHUNK_SIZE - 1);
			regionConnectivity[4] |= cell.z == 0;
			regionConnectivity[5] |= cell.z == (CHUNK_SIZE - 1);
		
			// Spread neighbors
			for (int i = 0; i < 6; i++)
			{
				glm::ivec3 neighborPos = cell + dirs[i];
				
				// Check if neighbor is in boundaries
				glm::ivec3 truncated = neighborPos & CHUNK_UPPER_BITS_MASK;
				if (!(truncated.x == 0 && truncated.y == 0 && truncated.z == 0))
				{
					continue;
				}
				size_t neighborIndex = getIndex(neighborPos.x, neighborPos.y, neighborPos.z);

				// Check if neighbor is visited
				if (visitedCells[neighborIndex])
				{
					continue;
				}
				visitedCells[neighborIndex] = true;

				// Check if neighbor is in opaque block
				Block block = blocks[neighborIndex];
				if (!GET_BLOCK_PROPERTIES(block).areFacesTransparent)
				{
					continue;
				}

				cellsToVisit.push_back(neighborPos);
			}
		}

		// Region is filled
		for (int i = 0; i < 5; i++)
		{
			for (int j = i + 1; j < 6; j++)
			{
				chunkConnectivity.set(i, j, true);
			}
		}
	}

	// Check flood fill mask if it filled all the space
}

void Chunk::removeIndexFromMap(Block block, uint16_t idx)
{
	auto it = changedBlocks.find(block);
	if (it == changedBlocks.end()) return;

	auto& vec = it->second;

	for (size_t i = 0; i < vec.size(); i++)
	{
		if (vec[i] == idx) {
			vec[i] = vec.back();  // swap with last
			vec.pop_back();       // remove last
			break;
		}
	}

	if (vec.empty()) changedBlocks.erase(it);
}

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

	//
	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };

	// Initialize all light values to 0
	std::fill(std::begin(lightLevels), std::end(lightLevels), LightLevel(0, 0));

	// Step 1: Collect block light sources
	std::queue<LightNode> localBlockLightBfsQueue;
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					size_t index = getIndex(x, y, z);

					Block currentBlock = blocks[index];

					uint8_t emission = GET_BLOCK_PROPERTIES(currentBlock).lightEmission;
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
	const ChunkColumnData* chunkColumnData = TerrainGenerator::getInstance().getChunkColumnData(position.x, position.z);
	const int* heightMap = chunkColumnData->heightMapRead();

	std::queue<LightNode> localSkyLightBfsQueue;
	const Chunk* top = neighbors[3];
	{
		if (top && top->getState() > State::BuildingLight)
		{
			// Propagate from top neighbor
			const int y = CHUNK_SIZE - 1;
			const int neighborY = 0;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					size_t index = getIndex(x, y, z);
					if (GET_BLOCK_PROPERTIES(blocks[index]).absorbsLight)
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
				if (GET_BLOCK_PROPERTIES(blocks[index]).absorbsLight)
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

		// +Y. Sky light shouldn't be gropagated if 'top' chunk isn't nullptr, but I don't care much.
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
		// TODO: Consider using vector to speed up. Must remove front element!
		while (!localBlockLightBfsQueue.empty())
		{
			// Get node data
			const auto& data = localBlockLightBfsQueue.front();
			int x = data.x;
			int y = data.y;
			int z = data.z;
			size_t index = getIndex(x, y, z);
			localBlockLightBfsQueue.pop();

			// Get light level
			uint8_t blockLight = lightLevels[index].blockLight;
			if (blockLight < 2)
			{
				continue;
			}
			uint8_t lightToSet = blockLight - 1;

			// Propagate to neighbors
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

				const Block neighborBlock = neighborChunk->getBlockAt(neighborIndex);
				if (GET_BLOCK_PROPERTIES(neighborBlock).absorbsLight)
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
		// TODO: Consider using vector to speed up. Must remove front element!
		while (!localSkyLightBfsQueue.empty())
		{
			// Get node data
			const auto& data = localSkyLightBfsQueue.front();
			int x = data.x;
			int y = data.y;
			int z = data.z;
			size_t index = getIndex(x, y, z);
			localSkyLightBfsQueue.pop();

			// Get light level
			uint8_t skyLight = lightLevels[index].skyLight;
			if (skyLight < 2)
			{
				continue;
			}

			// Propagate to neighbors
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

				const Block neighborBlock = neighborChunk->getBlockAt(neighborIndex);
				if (GET_BLOCK_PROPERTIES(neighborBlock).absorbsLight)
				{
					continue;
				}

				// If we are propagating down and skyLight is 15, lightAbsorption is 0, otherwise 1
				uint8_t neighborSkyLight = neighborChunk->getLightAt(neighborIndex).skyLight;

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

	std::queue<LightNode> localBlockLightBfsQueue;
	{
		std::lock_guard<std::mutex> lock(blockLightBfsMutex);
		localBlockLightBfsQueue.swap(blockLightBfsQueue);
	}
	std::queue<LightRemovalNode> localBlockLightRemovalBfsQueue;
	{
		std::lock_guard<std::mutex> lock(blockLightRemovalBfsMutex);
		localBlockLightRemovalBfsQueue.swap(blockLightRemovalBfsQueue);
	}
	std::queue<LightNode> localSkyLightBfsQueue;
	{
		std::lock_guard<std::mutex> lock(skyLightBfsMutex);
		localSkyLightBfsQueue.swap(skyLightBfsQueue);
	}
	std::queue<LightRemovalNode> localSkyLightRemovalBfsQueue;
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

	//TODO: Instead of immediate neighbor calls, collect and batch

	// Remove block light
	while (!localBlockLightRemovalBfsQueue.empty())
	{
		// Get node data
		const auto& data = localBlockLightRemovalBfsQueue.front();
		int x = data.x;
		int y = data.y;
		int z = data.z;
		uint8_t nodeLightLevel = data.lightLevel;
		size_t index = getIndex(x, y, z);
		localBlockLightRemovalBfsQueue.pop();

		// Propagate to neighbors
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

			Block neighborBlock = neighborChunk->getBlockAt(neighborIndex);
			if (GET_BLOCK_PROPERTIES(neighborBlock).absorbsLight)
			{
				continue;
			}

			uint8_t neighborBlockLight = neighborChunk->getLightAt(neighborIndex).blockLight;
			if (neighborBlockLight > 0 && neighborBlockLight < nodeLightLevel)
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
			else if (neighborBlockLight >= nodeLightLevel)
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
	// TODO: Consider using vector to speed up. Must remove front element!
	while (!localBlockLightBfsQueue.empty())
	{
		// Get node data
		const auto& data = localBlockLightBfsQueue.front();
		int x = data.x;
		int y = data.y;
		int z = data.z;
		size_t index = getIndex(x, y, z);
		localBlockLightBfsQueue.pop();

		// Get light level
		uint8_t blockLight = lightLevels[index].blockLight;
		if (blockLight < 2)
		{
			continue;
		}
		uint8_t lightToSet = blockLight - 1;

		// Propagate to neighbors
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

			Block neighborBlock = neighborChunk->getBlockAt(neighborIndex);
			if (GET_BLOCK_PROPERTIES(neighborBlock).absorbsLight)
			{
				continue;
			}

			uint8_t neighborBlockLight = neighborChunk->getLightAt(neighborIndex).blockLight;
			if (neighborBlockLight >= lightToSet)
			{
				continue;
			}

			int checkX = nx & CHUNK_UPPER_BITS_MASK;
			int checkY = ny & CHUNK_UPPER_BITS_MASK;
			int checkZ = nz & CHUNK_UPPER_BITS_MASK;

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
		const auto& data = localSkyLightRemovalBfsQueue.front();
		int x = data.x;
		int y = data.y;
		int z = data.z;
		uint8_t nodeLightLevel = data.lightLevel;
		size_t index = getIndex(x, y, z);
		localSkyLightRemovalBfsQueue.pop();

		// Propagate to neighbors
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

			Block neighborBlock = neighborChunk->getBlockAt(neighborIndex);
			if (GET_BLOCK_PROPERTIES(neighborBlock).absorbsLight)
			{
				continue;
			}

			uint8_t neighborSkyLight = neighborChunk->getLightAt(neighborIndex).skyLight;
			if (neighborSkyLight > 0 &&
				(neighborSkyLight < nodeLightLevel || (nodeLightLevel == 15 && i == 2)))
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
			else if (neighborSkyLight >= nodeLightLevel)
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
		const auto& data = localSkyLightBfsQueue.front();
		int x = data.x;
		int y = data.y;
		int z = data.z;
		size_t index = getIndex(x, y, z);
		localSkyLightBfsQueue.pop();

		// Get light level
		uint8_t skyLight = lightLevels[index].skyLight;
		if (skyLight < 2)
		{
			continue;
		}

		// Propagate to neighbors
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

			Block neighborBlock = neighborChunk->getBlockAt(neighborIndex);
			if (GET_BLOCK_PROPERTIES(neighborBlock).absorbsLight)
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
	{
		std::lock_guard<std::mutex> lock(blockLightBfsMutex);
		if (!blockLightBfsQueue.empty())
		{
			return true;
		}
	}
	{
		std::lock_guard<std::mutex> lock(blockLightRemovalBfsMutex);
		if (!blockLightRemovalBfsQueue.empty())
		{
			return true;
		}
	}
	{
		std::lock_guard<std::mutex> lock(skyLightBfsMutex);
		if (!skyLightBfsQueue.empty())
		{
			return true;
		}
	}
	{
		std::lock_guard<std::mutex> lock(skyLightRemovalBfsMutex);
		if (!skyLightRemovalBfsQueue.empty())
		{
			return true;
		}
	}
	return false;
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
		std::vector<BlockFaceInstance> opaqueInstances;
		std::vector<BlockFaceInstance> transparentInstances;
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					// Generate new faces for this block
					Block block = getBlockAt(x, y, z);
					const BlockData* blockData = GET_BLOCK_DATA(block);
					if (!blockData->properties.hasFaces)
					{
						continue;
					}

					const auto& textureIDs = blockData->textures.textureIDs;
					auto& instances = blockData->properties.areFacesTransparent ? transparentInstances : opaqueInstances;
					const BlockData* neighborBlockData;

					size_t neighborIndex;
					const Chunk* neighborChunk;
					Block neighborBlock;
					LightLevel neighborLight;

					// -X
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x - 1, y, z, 0, neighborIndex);
					if (neighborChunk)
					{
						neighborBlock = neighborChunk->getBlockAt(neighborIndex);
						if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
						{
							neighborLight = neighborChunk->getLightAt(neighborIndex);
							unsigned int ao, light;
							calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 0, neighborLight);
							instances.emplace_back(
								x, y, z,
								0,
								ao,
								textureIDs[0],
								blockData->textures.texturesTransformation & 3,
								light
							);
						}
					}

					// +X
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x + 1, y, z, 1, neighborIndex);
					if (neighborChunk)
					{
						neighborBlock = neighborChunk->getBlockAt(neighborIndex);
						if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
						{
							neighborLight = neighborChunk->getLightAt(neighborIndex);
							unsigned int ao, light;
							calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 1, neighborLight);
							instances.emplace_back(
								x, y, z,
								1,
								ao,
								textureIDs[1],
								(blockData->textures.texturesTransformation >> 2) & 3,
								light);
						}
					}

					// -Y
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x, y - 1, z, 2, neighborIndex);
					if (neighborChunk)
					{
						neighborBlock = neighborChunk->getBlockAt(neighborIndex);
						if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
						{
							neighborLight = neighborChunk->getLightAt(neighborIndex);
							unsigned int ao, light;
							calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 2, neighborLight);
							instances.emplace_back(
								x, y, z,
								2,
								ao,
								textureIDs[2],
								(blockData->textures.texturesTransformation >> 4) & 3,
								light
							);
						}
					}

					// +Y
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x, y + 1, z, 3, neighborIndex);
					if (neighborChunk)
					{
						neighborBlock = neighborChunk->getBlockAt(neighborIndex);
						if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
						{
							neighborLight = neighborChunk->getLightAt(neighborIndex);
							unsigned int ao, light;
							calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 3, neighborLight);
							instances.emplace_back(
								x, y, z,
								3,
								ao,
								textureIDs[3],
								(blockData->textures.texturesTransformation >> 6) & 3,
								light
							);
						}
					}

					// -Z
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x, y, z - 1, 4, neighborIndex);
					if (neighborChunk)
					{
						neighborBlock = neighborChunk->getBlockAt(neighborIndex);
						if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
						{
							neighborLight = neighborChunk->getLightAt(neighborIndex);
							unsigned int ao, light;
							calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 4, neighborLight);
							instances.emplace_back(
								x, y, z,
								4,
								ao,
								textureIDs[4],
								(blockData->textures.texturesTransformation >> 8) & 3,
								light
							);
						}
					}

					// +Z
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x, y, z + 1, 5, neighborIndex);
					if (neighborChunk)
					{
						neighborBlock = neighborChunk->getBlockAt(neighborIndex);
						if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
						{
							neighborLight = neighborChunk->getLightAt(neighborIndex);
							unsigned int ao, light;
							calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 5, neighborLight);
							instances.emplace_back(
								x, y, z,
								5,
								ao,
								textureIDs[5],
								(blockData->textures.texturesTransformation >> 10) & 3,
								light);
						}
					}
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
		meshData.opaqueDirty = false;
		meshData.transparentDirty = false;

		meshData.opaqueInstances = std::move(opaqueInstances);
		meshData.transparentInstances = std::move(transparentInstances);

		if (meshData.getOpaqueFaceCount() > 0)
		{
			meshData.opaqueDirty = true;
		}
		if (meshData.getTransparentFaceCount() > 0)
		{
			meshData.transparentDirty = true;
			if (meshData.getTransparentFaceCount() > 1)
			{
				shouldSortMeshAfterBuild = true;
				cameraClosestBlockPosForSortingMesh = -1;
			}
		}

		// Update render face count if no changes (means no faces at all)
		if (!(meshData.opaqueDirty || meshData.transparentDirty))
		{
			meshData.updateRenderFaceCount();
		}
	}
}

void Chunk::sortMesh(const glm::ivec3& cameraBlockPos)
{
	shouldSortMeshAfterBuild = false;

	const glm::ivec3 chunkGlobalPos = position * CHUNK_SIZE;

	const glm::ivec3 calculateDistanceFrom = glm::clamp(cameraBlockPos - chunkGlobalPos, glm::ivec3(0), glm::ivec3(CHUNK_SIZE - 1));
	const uint16_t compare = calculateDistanceFrom.x |
							 (calculateDistanceFrom.y << CHUNK_SIZE_LOG2) |
							 (calculateDistanceFrom.z << (CHUNK_SIZE_LOG2 << 1));
	if (compare == cameraClosestBlockPosForSortingMesh)
	{
		return;
	}
	cameraClosestBlockPosForSortingMesh = compare;

	ScopedProcessingFence scopedFence(meshData.processingFence);

	PROFILE_SCOPE("Sort chunk mesh", ProfileCategory::ChunkMesh);

	// Sorting only transparent faces for now
	// If GL_CULL_FACE is disabled, sorting must be adjusted. If faces have same position, they should be sorted by prioritized normal(something opposite to camera direction).
	// TODO: Consider sorting opaque faces to reduce overdraw

	// Create buckets
	constexpr int BUCKET_COUNT = (CHUNK_SIZE - 1) * 3 + 1;
	size_t bucketOffsets[BUCKET_COUNT + 1];
	std::fill(bucketOffsets, bucketOffsets + BUCKET_COUNT + 1, 0);

	for (const auto& instance : meshData.transparentInstances)
	{
		glm::ivec3 pos;
		instance.decodePosition(pos.x, pos.y, pos.z);
		glm::ivec3 delta = glm::abs(pos - calculateDistanceFrom);
		uint8_t manhattanDistance = delta.x + delta.y + delta.z;
		bucketOffsets[manhattanDistance + 1]++;
	}

	// Prefix sum (Last elemnt isn't needed, so don't change it)
	for (size_t i = 1; i < BUCKET_COUNT; i++)
	{
		bucketOffsets[i] += bucketOffsets[i - 1];
	}

	// Second pass: place instances directly in final positions
	std::vector<BlockFaceInstance> sorted(meshData.transparentInstances.size());
	for (const auto& instance : meshData.transparentInstances)
	{
		glm::ivec3 pos;
		instance.decodePosition(pos.x, pos.y, pos.z);
		glm::ivec3 delta = glm::abs(pos - calculateDistanceFrom);
		uint8_t manhattanDistance = delta.x + delta.y + delta.z;
		sorted[bucketOffsets[manhattanDistance]++] = instance;
	}
	meshData.transparentInstances = std::move(sorted);
	meshData.transparentDirty = true;
}

bool Chunk::shouldMeshBeSorted(bool cameraMoved) const
{
	return meshData.getTransparentFaceCount() > 1 && (cameraMoved || shouldSortMeshAfterBuild);
}

bool Chunk::shouldMeshBeUpdated() const
{
	return meshDirty && isLightBuilt();
}

void Chunk::markMeshDirty()
{
	meshDirty = true;
}

void Chunk::askForMeshUpload()
{
	if (meshData.opaqueDirty || meshData.transparentDirty)
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

	for (MeshData* chunkMesh : pendingMeshUploads)
	{
		chunkMesh->processingFence.startProcessing();
	}

	PROFILE_SCOPE("Send chunk meshes to GPU", ProfileCategory::ChunkMesh);

	// Allocate memory for meshes
	std::vector<MeshData*> allocateMemoryMeshRequests;
	for (MeshData* chunkMesh : pendingMeshUploads)
	{
		// TODO: Maybe first check is redundant
		if (!chunkMesh->created)
		{
			// No mesh
			allocateMemoryMeshRequests.push_back(chunkMesh);
		}
		else if (chunkMesh->getFaceCount() > chunkMesh->getFaceCapacity())
		{
			// Asking for more place
			allocateMemoryMeshRequests.push_back(chunkMesh);
		}
	}

	auto& inst = ChunkMeshManager::getInstance();
	inst.processMeshRequests(allocateMemoryMeshRequests);

	// Write meshes data
	auto& instanceVBO = inst.getInstanceVBO();
	instanceVBO.bind();

	for (MeshData* chunkMesh : pendingMeshUploads)
	{
		if (!chunkMesh->created)
		{
			continue;
		}

		size_t opaqueFaceCount = chunkMesh->getOpaqueFaceCount();
		size_t transparentFaceCount = chunkMesh->getTransparentFaceCount();

		size_t offset = chunkMesh->allocatedBlock.offset * sizeof(BlockFaceInstance);

		if (chunkMesh->opaqueDirty)
		{
			instanceVBO.write(
				chunkMesh->opaqueInstances.data(),
				opaqueFaceCount * sizeof(BlockFaceInstance),
				offset
			);
			chunkMesh->opaqueDirty = false;
		}

		if (chunkMesh->transparentDirty)
		{
			instanceVBO.write(
				chunkMesh->transparentInstances.data(),
				transparentFaceCount * sizeof(BlockFaceInstance),
				offset + opaqueFaceCount * sizeof(BlockFaceInstance)
			);
			chunkMesh->transparentDirty = false;
		}

		chunkMesh->updateRenderFaceCount();
	}

	//
	for (MeshData* chunkMesh : pendingMeshUploads)
	{
		chunkMesh->processingFence.stopProcessing();
	}
	pendingMeshUploads.clear();
}

void Chunk::collectOpaqueRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const
{
	size_t faceCount = meshData.renderOpaqueFaceCount;
	if (faceCount == 0)
	{
		return;
	}
	drawCommands.emplace_back(4, faceCount, 0, meshData.allocatedBlock.offset);
	positions.push_back(position);
}

void Chunk::collectTransparentRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const
{
	size_t faceCount = meshData.renderTransparentFaceCount;
	if (faceCount == 0)
	{
		return;
	}
	drawCommands.emplace_back(4, faceCount, 0, meshData.allocatedBlock.offset + meshData.renderOpaqueFaceCount);
	positions.push_back(position);
}

bool Chunk::canBeRendered() const
{
	return meshData.created && meshData.getRenderFaceCount() > 0;
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

Block Chunk::getBlockAt(int x, int y, int z) const
{
	ASSERT(((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0);
	return blocks[getIndex(x, y, z)];
}

LightLevel Chunk::getLightAt(int x, int y, int z) const
{
	ASSERT(((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0);
	return lightLevels[getIndex(x, y, z)];
}

std::pair<Block, LightLevel> Chunk::getBlockAndLightAt(int x, int y, int z) const
{
	ASSERT(((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0);
	size_t index = getIndex(x, y, z);
	return std::make_pair(blocks[index], lightLevels[index]);
}

Block Chunk::getBlockAt(size_t index) const
{
	ASSERT(index < CHUNK_VOLUME);
	return blocks[index];
}

LightLevel Chunk::getLightAt(size_t index) const
{
	ASSERT(index < CHUNK_VOLUME);
	return lightLevels[index];
}

std::pair<Block, LightLevel> Chunk::getBlockAndLightAt(size_t index) const
{
	ASSERT(index < CHUNK_VOLUME);
	return std::make_pair(blocks[index], lightLevels[index]);
}

void Chunk::setBlockAt(int x, int y, int z, Block block, bool saveBlockChanges)
{
	ASSERT(((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0);

	size_t index = getIndex(x, y, z);

	Block previousBlock = blocks[index];
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

	// Mark meshes as dirty
	markBlockMeshDirty(x, y, z);

	// Light update
	const BlockData* previousBlockData = BlockDataBase::getBlockData(previousBlock);
	uint8_t previousEmission = previousBlockData->properties.lightEmission;

	const BlockData* newBlockData = BlockDataBase::getBlockData(block);
	uint8_t newEmission = newBlockData->properties.lightEmission;

	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };
	if (previousBlockData->properties.absorbsLight && !newBlockData->properties.absorbsLight)
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
	else if (!previousBlockData->properties.absorbsLight && newBlockData->properties.absorbsLight)
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
}

void Chunk::setLightAt(int x, int y, int z, LightLevel lightValue)
{
	ASSERT(((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0);
	lightLevels[getIndex(x, y, z)] = lightValue;
	markBlockMeshDirty(x, y, z);
}

void Chunk::setBlockLightAt(int x, int y, int z, uint8_t lightLevel)
{
	ASSERT(((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0);
	lightLevels[getIndex(x, y, z)].blockLight = lightLevel;
	markBlockMeshDirty(x, y, z);
}

void Chunk::setSkyLightAt(int x, int y, int z, uint8_t lightLevel)
{
	ASSERT(((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0);
	lightLevels[getIndex(x, y, z)].skyLight = lightLevel;
	markBlockMeshDirty(x, y, z);
}

void Chunk::setLightAt(size_t index, LightLevel lightValue)
{
	ASSERT(index < CHUNK_VOLUME);
	lightLevels[index] = lightValue;

	glm::ivec3 pos = getPositionFromIndex(index);
	markBlockMeshDirty(pos.x, pos.y, pos.z);
}

void Chunk::setBlockLightAt(size_t index, uint8_t lightLevel)
{
	ASSERT(index < CHUNK_VOLUME);
	lightLevels[index].blockLight = lightLevel;

	glm::ivec3 pos = getPositionFromIndex(index);
	markBlockMeshDirty(pos.x, pos.y, pos.z);
}

void Chunk::setSkyLightAt(size_t index, uint8_t lightLevel)
{
	ASSERT(index < CHUNK_VOLUME);
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

	// Mark neighbors meshes as dirty

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

void Chunk::calculateVertexAmbientOcclusionAndLight(unsigned int& ao, LightLevel& light, LightLevel centerLight, LightLevel side1Light, LightLevel side2Light, LightLevel cornerLight, bool side1Solid, bool side2Solid, bool cornerSolid) const
{
	unsigned int blockLightSum = centerLight.blockLight;
	unsigned int skyLightSum = centerLight.skyLight;
	unsigned int count = 1;

	if (side1Solid && side2Solid)
	{
		ao = 0;
		light = centerLight;
		return;
	}

#if CHUNK_SMOOTH_LIGHTING
	if (!side1Solid)
	{
		blockLightSum += side1Light.blockLight;
		skyLightSum += side1Light.skyLight;
		count++;
	}

	if (!side2Solid)
	{
		blockLightSum += side2Light.blockLight;
		skyLightSum += side2Light.skyLight;
		count++;
	}

	if (!cornerSolid)
	{
		blockLightSum += cornerLight.blockLight;
		skyLightSum += cornerLight.skyLight;
		count++;
	}
#endif

	unsigned int avgBlockLight = blockLightSum / count;
	unsigned int avgSkyLight = skyLightSum / count;

	ASSERT(avgBlockLight >= 0 && avgBlockLight <= 15);
	ASSERT(avgSkyLight >= 0 && avgSkyLight <= 15);

	ao = 3 - (side1Solid + side2Solid + cornerSolid);
	light.blockLight = avgBlockLight;
	light.skyLight = avgSkyLight;
}

void Chunk::calculateFaceAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, int x, int y, int z, int normal, LightLevel centerFaceLight) const
{
	// For each face normal, we need to check 8 neighbors around the face
	// The AO calculation depends on which direction the face is facing

	std::pair<Block, LightLevel> data[8];
	bool n[8]; // 8 neighbors around the face

	unsigned int ao0, ao1, ao2, ao3;
	LightLevel lightLevels[4];

	auto getSafe = [this, &data, &n](size_t dataIdx, int x_, int y_, int z_)
		{
			size_t idx;
			const Chunk* c = getChunkAndIndex_checkNeighborsTraverse(x_, y_, z_, idx);

			n[dataIdx] = true;
			if (c)
			{
				data[dataIdx] = c->getBlockAndLightAt(idx);
				n[dataIdx] = !GET_BLOCK_PROPERTIES(data[dataIdx].first).areFacesTransparent;
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

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
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

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
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

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
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

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, data[4].second, data[1].second, data[2].second, n[4], n[1], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, data[3].second, data[1].second, data[0].second, n[3], n[1], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, data[3].second, data[6].second, data[5].second, n[3], n[6], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, data[4].second, data[6].second, data[7].second, n[4], n[6], n[7]);
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

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
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

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		break;
	}

	//
	ao = ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6);
	light = *((unsigned int*)lightLevels);
}

glm::ivec3 Chunk::getPosition() const
{
	return position;
}

size_t Chunk::getFaceCount() const
{
	return meshData.getFaceCount();
}

size_t Chunk::getFaceCapacity() const
{
	return meshData.getFaceCapacity();
}

Chunk::State Chunk::getState() const
{
	return state.load(std::memory_order_acquire);
}

void Chunk::setState(State newState)
{
	state.store(newState, std::memory_order_release);
}

bool Chunk::getIsProcessing() const
{
	return processingFence.isProcessing();// || meshData.processingFence.isProcessing();
}

bool Chunk::getIsLoadedInWorld() const
{
	return chunkFlags.read(Flag::IsLoadedInWorld);
}

bool Chunk::areBlocksBuilt() const
{
	return getState() >= State::BlocksBuilt;
}

bool Chunk::isLightBuilt() const
{
	return getState() >= State::LightsBuilt;
}

void Chunk::setBlocksBuiltToFalse()
{
	setState(State::NotInitialized_NeedsBlocks);
	changedBlocks.clear();
}

void Chunk::addLoader()
{
	loaderCount++;
}

void Chunk::removeLoader()
{
	loaderCount--;
}

uint8_t Chunk::getLoaderCount() const
{
	return loaderCount;
}

//============================================================================
//LightLevel

LightLevel::LightLevel() :
	blockLight(0), skyLight(0)
{
}

LightLevel::LightLevel(uint8_t blockLight, uint8_t skyLight) :
	blockLight(blockLight), skyLight(skyLight)
{
}

LightLevel::LightLevel(const LightLevel& other) :
	blockLight(other.blockLight), skyLight(other.skyLight)
{
}

LightLevel& LightLevel::operator=(const LightLevel& other)
{
	blockLight = other.blockLight;
	skyLight = other.skyLight;
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
