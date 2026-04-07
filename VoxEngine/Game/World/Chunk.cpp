#include "Chunk.h"

#include "TerrainGenerator.h"

#include "Game/DataPackManagment/AssetRegistry.h"

#include "Core/Profiler.h"
#include "Core/Assert.h"
#include "Core/Hashes/ivec2Hasher.h"

thread_local ChunkMeshFaceStorage::InstancesStorage Chunk::localMeshInstances;

std::atomic<bool> Chunk::gHasStructureBlockChanges{ false };
std::unique_ptr<ChunkRegionManager> Chunk::chunkRegionManagerInstance;

Chunk::CachedBlockIds Chunk::CACHED_BLOCK_IDS;

StructureBlockChangeManager Chunk::structureBlockChangeManager;


static uint32_t hash3(uint32_t x, uint32_t y, uint32_t z)
{
	uint32_t data = x * 0x27d4eb2du + y * 0x165667b1u + z * 0x1b873593u;
	data ^= data >> 15u;
	data *= 0x85ebca6bu;
	data ^= data >> 13u;
	data *= 0xc2b2ae35u;
	data ^= data >> 16u;
	return data;
}

// Prepares chunk for use
void Chunk::init(const glm::ivec3& position, const std::array<Chunk*, 27>& newNeighbors, ChunkRegion* parentRegion)
{
	PROFILE_SCOPE("Chunk init", ProfileCategory::ChunkLoadUnload);

	// Set position
	this->position = position;

	// Set neighbors
	this->neighbors = newNeighbors;
	constexpr int selfIndex = getNeighborIndex(0, 0, 0);
	neighbors[selfIndex] = this; // Set self pointer for easier access. This also allows to use the same indexing for neighbors and self.
	for (int i = 0; i < newNeighbors.size(); i++)
	{
		if (i == selfIndex) continue;

		Chunk* neighbor = this->neighbors[i];
		if (neighbor)
		{
			neighbor->neighbors[getOppositeNeighborIndex(i)] = this;
		}
	}

	// Set parent region
	this->parentRegion = parentRegion;

	// Reset state and flags
	setState(Chunk::State::NotInitialized_NeedsBlocks);

	chunkFlags.reset();
	chunkFlags.set(Flag::IsLoadedInWorld, true);

	// Reset mesh data
	mesh.faceStorage.resetRenderFaceCount();
	mesh.setFlag(ChunkMesh::Flag::ShouldBeUploaded, false);

	// Reset grid data
	std::memset(blocks, CACHED_BLOCK_IDS.airId, sizeof(blocks));
	std::memset(lightLevels, 0, sizeof(lightLevels));
}

// Cleans up resources
void Chunk::destroy()
{
	PROFILE_SCOPE("Chunk destroy", ProfileCategory::ChunkLoadUnload);

	// Set states and flags
	setFlag(Flag::IsLoadedInWorld, false);
	setState(Chunk::State::NotInitialized_NeedsBlocks);

	// Clear mesh data
	{
		if (readFlag(Flag::CanBeRendered))
		{
			parentRegion->decrementRenderChunkCount();
		}

		FenceGuard scopedFence(mesh.faceStorage.processingFence);
		mesh.faceStorage.clearInstances();
	}

	// Release chunk column data
	if (chunkFlags.read(Flag::IsLoadedChunkColumnData))
	{
		chunkFlags.set(Flag::IsLoadedChunkColumnData, false);
		TerrainGenerator::getInstance().unloadChunkColumnData(position.x, position.z);
	}

	// Put fence guard here
	{
		FenceGuard fence(processingFence);

		// Clear neighbors
		constexpr int selfIndex = getNeighborIndex(0, 0, 0);
		neighbors[selfIndex] = nullptr;
		for (int i = 0; i < neighbors.size(); i++)
		{
			if (i == selfIndex) continue;

			Chunk* neighbor = neighbors[i];
			if (neighbor)
			{
				neighbor->neighbors[getOppositeNeighborIndex(i)] = nullptr;
				neighbors[i] = nullptr;
			}
		}

		// Clear light queues
		lightPropagation.clear();
	}

	// TODO: Make it async. Mark chunk as processing.
	save();
	blockChanges.clear();
}

void Chunk::globalInit()
{
	// Create chunk region manager instance
	chunkRegionManagerInstance = std::make_unique<ChunkRegionManager>();

	// Cache block ids
	CACHED_BLOCK_IDS.airId = AssetRegistry::getBlockNumericalId("core:air");
	CACHED_BLOCK_IDS.waterId = AssetRegistry::getBlockNumericalId("core:water");
	CACHED_BLOCK_IDS.grassBlockId = AssetRegistry::getBlockNumericalId("core:grass_block");
	CACHED_BLOCK_IDS.dirtId = AssetRegistry::getBlockNumericalId("core:dirt");
	CACHED_BLOCK_IDS.stoneId = AssetRegistry::getBlockNumericalId("core:stone");
	CACHED_BLOCK_IDS.oakLogId = AssetRegistry::getBlockNumericalId("core:oak_log");
	CACHED_BLOCK_IDS.oakLeavesId = AssetRegistry::getBlockNumericalId("core:oak_leaves");
}

void Chunk::globalDestroy()
{
	// Destroy chunk region manager instance
	chunkRegionManagerInstance.reset();
}

void Chunk::buildBlocks()
{
	if (!chunkFlags.read(Flag::IsLoadedInWorld))
	{
		return;
	}

	FenceGuard scopedFence(processingFence);

	// Load chunk column data
	const ChunkColumnData* chunkColumnData;
	chunkColumnData = TerrainGenerator::getInstance().loadChunkColumnData(position.x, position.z);
	chunkFlags.set(Flag::IsLoadedChunkColumnData, true);
	const int* heightMap = chunkColumnData->heightMapRead();

	// Terrain
	bool computeCaveMask = false;

	constexpr int OCEAN_LEVEL = 0;

	const int globalChunkY = position.y * CHUNK_SIZE;

	const bool isInTerrainRange = globalChunkY <= chunkColumnData->getMaxHeight();
	const bool isInWaterRange = globalChunkY <= OCEAN_LEVEL;

	if (isInTerrainRange || isInWaterRange)
	{
		PROFILE_SCOPE("Build terrain", ProfileCategory::ChunkBlocks);

		// Important to keep layers connected, so 'index' won't go out of sync
		const int globalChunkY = position.y * CHUNK_SIZE;
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				// Compute global height
				const int globalHeight = heightMap[z + (x << CHUNK_SIZE_LOG2)];

				// Compute ranges
				const int surfaceY = globalHeight - globalChunkY;
				const int dirtY = surfaceY - 4;

				const int stoneEnd = std::min(CHUNK_SIZE, dirtY);

				const int dirtStart = std::max(0, dirtY);
				const int dirtEnd = std::min(CHUNK_SIZE, surfaceY);

				const bool hasSurface = surfaceY >= 0 && surfaceY < CHUNK_SIZE;

				const int waterStart = std::max(0, surfaceY + 1);
				const int waterEnd = std::min(CHUNK_SIZE, OCEAN_LEVEL - globalChunkY + 1);

				const int airStart = std::max(waterStart, waterEnd);

				// Stone
				computeCaveMask |= stoneEnd > 0;
				size_t index = getIndex(x, 0, z);
				for (int y = 0; y < stoneEnd; y++)
				{
					blocks[index] = CACHED_BLOCK_IDS.stoneId;
					index += CoordinatesStride3D::y;
				}

				// Dirt
				computeCaveMask |= dirtEnd > dirtStart;
				index = getIndex(x, dirtStart, z);
				for (int y = dirtStart; y < dirtEnd; y++)
				{
					blocks[index] = CACHED_BLOCK_IDS.dirtId;
					index += CoordinatesStride3D::y;
				}

				// Grass
				if (hasSurface)
				{
					index = getIndex(x, surfaceY, z);
					blocks[index] = CACHED_BLOCK_IDS.grassBlockId;
					computeCaveMask = true;
				}

				// Water
				computeCaveMask |= waterEnd > waterStart;
				index = getIndex(x, waterStart, z);
				for (int y = waterStart; y < waterEnd; y++)
				{
					blocks[index] = CACHED_BLOCK_IDS.waterId;
					index += CoordinatesStride3D::y;
				}

				// Air
				index = getIndex(x, airStart, z);
				for (int y = airStart; y < CHUNK_SIZE; y++)
				{
					blocks[index] = CACHED_BLOCK_IDS.airId;
					index += CoordinatesStride3D::y;
				}
			}
		}
	}

	// Caves
	if (computeCaveMask)
	{
		alignas(SimdF::bytes) bool caveMask[CHUNK_VOLUME];
		TerrainGenerator::getInstance().computeCaveMask(caveMask, position.x, position.y, position.z);

		PROFILE_SCOPE("Generate caves", ProfileCategory::ChunkBlocks);

		for (int i = 0; i < CHUNK_VOLUME; i++)
		{
			if (caveMask[i])
			{
				blocks[i] = CACHED_BLOCK_IDS.airId;
			}
		}
	}

	// Trees
	// TODO: Fix, trees spawning in air
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
				if (blocks[rootIndex] != CACHED_BLOCK_IDS.airId)
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
			if (!change.placeIfBlockIsAir || blocks[change.index] == CACHED_BLOCK_IDS.airId)
			{
				blocks[change.index] = change.block;
			}
		}
	}

	// Load blocks
	loadSave();

	//
	if (!chunkFlags.read(Flag::IsLoadedInWorld))
	{
		return;  // Chunk was unloaded during building
	}

	//
	setState(Chunk::State::BlocksBuit_NeedsLight);
}

void Chunk::updateStructureBlocks()
{
	PROFILE_SCOPE("Update chunk structure blocks", ProfileCategory::ChunkBlocks);

	FenceGuard scopedFence(processingFence);

	auto pendingChanges = structureBlockChangeManager.retrieveAndClearChanges(position);
	for (const auto& change : pendingChanges)
	{
		if (!change.placeIfBlockIsAir || blocks[change.index] == CACHED_BLOCK_IDS.airId)
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
		blocks[index] = CACHED_BLOCK_IDS.oakLogId;
	}
	
	// Leaves - create a spherical canopy
	int leavesStart = rootPosition.y + treeHeight - 2;
	int leavesEnd = rootPosition.y + treeHeight + 2;

	bool hasReachedOtherChunk = false;
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

				float squaredDistance = dx * dx + dz * dz + dy * dy * 0.8f; // Slightly elliptical

				if (squaredDistance > 4.0f)
				{
					continue;
				}

				if (((lx | ly | lz) & CHUNK_UPPER_BITS_MASK) == 0)
				{
					size_t index = getIndex(lx, ly, lz);
					if (blocks[index] == CACHED_BLOCK_IDS.airId)
					{
						blocks[index] = CACHED_BLOCK_IDS.oakLeavesId;
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

					structureBlockChangeManager.addChange(chunkPos, CACHED_BLOCK_IDS.oakLeavesId, index, true);
					hasReachedOtherChunk = true;
				}
			}
		}
	}

	if (hasReachedOtherChunk)
	{
		gHasStructureBlockChanges.store(true, std::memory_order_release);
	}
}

//bool Chunk::findFloodFillStartIndex(uint16_t& startIndex, const bool* floodFillMask) const
//{
//	//for (uint16_t i = startIndex; i < CHUNK_VOLUME; i++)
	//{
	//	if (floodFillMask[i])
	//	{
	//		// Already visited
	//		continue;
	//	}
//
//	//	Block block = blocks[i];
	//	const BlockData* blockData = BlockRegistry::getBlockData(block);
	//	if (!blockData->properties.areFacesTransparent)
	//	{
	//		// Block isn't transparent
	//		continue;
	//	}
//
//	//	startIndex = i;
	//	return true;
	//}
	//return false;
//	return false;
//}
//
//void Chunk::computeConnectivity()
//{
//	//PROFILE_SCOPE("Compute chunk connectivity", ProfileCategory::General);
//
//	//constexpr glm::ivec3 dirs[6] =
	//{
	//	{-1, 0, 0}, {1, 0, 0},
	//	{0, -1, 0}, {0, 1, 0},
	//	{0, 0, -1}, {0, 0, 1}
	//};
//
//	//// Reset regions
	//bool visitedCells[CHUNK_VOLUME]; // TODO: Can be a bitset
	//std::fill(visitedCells, visitedCells + CHUNK_VOLUME, false);
//
//	//SymmetricBitMatrix<6> chunkConnectivity; // 6x6 matrix
	//chunkConnectivity.fill(false);
//
//	//uint16_t startIndex = 0;
//
//	//std::vector<glm::ivec3> cellsToVisit;
//
//	////static std::mutex mtx;
	////std::lock_guard<std::mutex> lock(mtx);
//
//	//while (true)
	//{
	//	// Find start index
	//	if (!findFloodFillStartIndex(startIndex, visitedCells))
	//	{
	//		break;
	//	}
//
//	//	glm::ivec3 startPos = getPositionFromIndex(startIndex);
	//	visitedCells[startIndex] = true; // Mark as visited
	//	startIndex++; // Increment, so 'findFloodFillStartIndex' will look immediately at next block
//
//	//	bool regionConnectivity[6] = { false, false, false, false, false, false }; // TODO: Can be a bitset
//
//	//	cellsToVisit.push_back(startPos);
	//	while (!cellsToVisit.empty())
	//	{
	//		// Get cell
	//		glm::ivec3 cell = cellsToVisit.back();
	//		cellsToVisit.pop_back();
//
//	//		// Check if cell is on chunk border
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
//
//	//			// Check if neighbor is visited
	//			if (visitedCells[neighborIndex])
	//			{
	//				continue;
	//			}
	//			visitedCells[neighborIndex] = true;
//
//	//			// Check if neighbor is in opaque block
	//			Block block = blocks[neighborIndex];
	//			if (!GET_BLOCK_PROPERTIES(block).areFacesTransparent)
	//			{
	//				continue;
	//			}
//
//	//			cellsToVisit.push_back(neighborPos);
	//		}
	//	}
//
//	//	// Region is filled
	//	for (int i = 0; i < 5; i++)
	//	{
	//		for (int j = i + 1; j < 6; j++)
	//		{
	//			chunkConnectivity.set(i, j, true);
	//		}
	//	}
	//}
//
//	//// Check flood fill mask if it filled all the space
//}

void Chunk::removeBlockChange(BlockId block, uint16_t idx)
{
	auto it = blockChanges.find(block);
	if (it == blockChanges.end()) return;

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

	if (vec.empty()) blockChanges.erase(it);
}

void Chunk::loadSave()
{
	// Skip the filesystem entirely if the region tells us this chunk has never been written to disk
	size_t indexInRegion = ChunkRegion::getChunkIndexInRegion(position);
	if (!parentRegion || !parentRegion->hasSavedData(indexInRegion)) return;
	ChunkIO::loadBlocks(blockChanges, blocks, parentRegion->getPosition(), indexInRegion);
}

void Chunk::save() const
{
	if (!parentRegion) return;

	bool hasChanges = !blockChanges.empty();

	size_t indexInRegion = ChunkRegion::getChunkIndexInRegion(position);
	parentRegion->setHasSavedData(indexInRegion, hasChanges);

	if (hasChanges)
	{
		ChunkIO::saveBlocks(blockChanges, parentRegion->getPosition(), indexInRegion);
	}
}

uint32_t Chunk::propagateBlockLight()
{
	uint32_t neighborDirtyMask = 0;
	while (!LightPropagationStorage::threadLocalBlockLightPropagation.empty())
	{
		// Get node data
		const auto data = LightPropagationStorage::threadLocalBlockLightPropagation.pop_and_return_unsafe();

		// Get light level at current block
		uint8_t blockLight = lightLevels[getIndex(data.x, data.y, data.z)].blockLight;
		if (blockLight < 2)
		{
			continue;
		}

		uint8_t lightToSet = blockLight - 1;

		// Propagate to neighbors
		for (int i = 0; i < 6; i++)
		{
			// Calculate neighbor block coordinates
			const glm::ivec3 offset = DirectionsTable::directionsXYZ[i];
			const int nx = data.x + offset.x;
			const int ny = data.y + offset.y;
			const int nz = data.z + offset.z;

			// Get neighbor chunk and block index
			size_t neighborBlockIndex;
			Chunk* neighborChunk;

			const bool isNeighborBlockInSameChunk = ((nx | ny | nz) & CHUNK_UPPER_BITS_MASK) == 0;
			if (isNeighborBlockInSameChunk)
			{
				neighborChunk = this;
				neighborBlockIndex = getIndex(nx, ny, nz);
			}
			else
			{
				neighborChunk = neighbors[getSideNeighborIndex(i)];
				if (!neighborChunk)
				{
					continue;
				}
				neighborBlockIndex = getIndex(nx & CHUNK_LOWER_BITS_MASK, ny & CHUNK_LOWER_BITS_MASK, nz & CHUNK_LOWER_BITS_MASK);
			}

			// Get neighbor block light level and compare it
			uint8_t neighborBlockLight = neighborChunk->getLightAt(neighborBlockIndex).blockLight;
			if (neighborBlockLight >= lightToSet)
			{
				continue;
			}

			// Get neighbor block data
			const BlockId neighborBlock = neighborChunk->getBlockAt(neighborBlockIndex);
			const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);

			// If block absorbs light, skip
			if (!neighborBlockData || neighborBlockData->absorbsLight)
			{
				continue;
			}

			// Propagate
			neighborChunk->lightLevels[neighborBlockIndex].blockLight = lightToSet;
			if (isNeighborBlockInSameChunk)
			{
				LightPropagationStorage::threadLocalBlockLightPropagation.emplace(nx, ny, nz);
			}
			else
			{
				neighborChunk->addBlockLightPropagationNode(nx & CHUNK_LOWER_BITS_MASK, ny & CHUNK_LOWER_BITS_MASK, nz & CHUNK_LOWER_BITS_MASK);
			}

			// Accumulate dirty mask
			neighborDirtyMask |= getNeighborDirtyMask(nx, ny, nz);
		}
	}

	return neighborDirtyMask;
}

uint32_t Chunk::propagateSkyLight()
{
	constexpr std::array<int, 4> horizontalDirections{ 0, 1, 4, 5 };

	uint32_t neighborDirtyMask = 0;

	auto tryPropagate = [&](int nx, int ny, int nz, uint8_t lightToSet, bool includeYInSameChunkCheck) -> bool
		{
			size_t neighborBlockIndex;
			Chunk* neighborChunk;

			const bool inSameChunk = includeYInSameChunkCheck
				? (((nx | ny | nz) & CHUNK_UPPER_BITS_MASK) == 0)
				: (((nx | nz) & CHUNK_UPPER_BITS_MASK) == 0);

			if (inSameChunk)
			{
				neighborChunk = this;
				neighborBlockIndex = getIndex(nx, ny, nz);
			}
			else
			{
				// Neighbor chunk lookup is only needed when we actually cross a boundary
				// For SOME reason its faster to recompute than to just pass the direction. I don't know why. It doesn't make any sense.
				const int neighborChunkSide = includeYInSameChunkCheck ? 
					(nx < 0) ? 0 : (nx >= CHUNK_SIZE ? 1 : (ny < 0 ? 2 : (ny >= CHUNK_SIZE ? 3 : (nz < 0 ? 4 : 5)))) :
					(nx < 0) ? 0 : (nx >= CHUNK_SIZE ? 1 : (nz < 0 ? 4 : 5));
				const int neighborChunkIndex = getSideNeighborIndex(neighborChunkSide);

				neighborChunk = neighbors[neighborChunkIndex];
				if (!neighborChunk)
					return false;

				neighborBlockIndex = getIndex(
					nx & CHUNK_LOWER_BITS_MASK,
					ny & CHUNK_LOWER_BITS_MASK,
					nz & CHUNK_LOWER_BITS_MASK
				);
			}

			auto& dstLight = neighborChunk->lightLevels[neighborBlockIndex];
			if (dstLight.skyLight >= lightToSet)
				return false;

			const BlockId neighborBlock = neighborChunk->blocks[neighborBlockIndex];
			const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
			if (!neighborBlockData || neighborBlockData->absorbsLight)
				return false;

			dstLight.skyLight = lightToSet;

			if (inSameChunk)
			{
				LightPropagationStorage::threadLocalSkyLightPropagation.emplace(nx, ny, nz);
			}
			else
			{
				neighborChunk->addSkyLightPropagationNode(
					nx & CHUNK_LOWER_BITS_MASK,
					ny & CHUNK_LOWER_BITS_MASK,
					nz & CHUNK_LOWER_BITS_MASK
				);
			}

			neighborDirtyMask |= getNeighborDirtyMask(nx, ny, nz);
			return true;
		};

	while (!LightPropagationStorage::threadLocalSkyLightPropagation.empty())
	{
		const auto data = LightPropagationStorage::threadLocalSkyLightPropagation.pop_and_return_unsafe();
		const int x = data.x;
		const int y = data.y;
		const int z = data.z;

		const size_t selfIndex = getIndex(x, y, z);
		const uint8_t skyLight = lightLevels[selfIndex].skyLight;
		if (skyLight < 2)
			continue;

		const uint8_t nextLight = static_cast<uint8_t>(skyLight - 1);

		// Fast vertical-down column fill when the source is full sunlight
		const bool isFullSunlight = skyLight == 15;
		if (isFullSunlight)
		{
			int ny = y;
			size_t belowIndex = selfIndex;

			const std::array<glm::ivec2, 4> nXZs =
			{
				glm::ivec2(x - 1, z    ),
				glm::ivec2(x + 1, z    ),
				glm::ivec2(x    , z - 1),
				glm::ivec2(x    , z + 1)
			};

			while (--ny >= 0)
			{
				belowIndex -= CoordinatesStride3D::y; // Move down one block in the column

				if (lightLevels[belowIndex].skyLight == 15)
					break;

				const BlockId block = blocks[belowIndex];
				const auto* blockData = AssetRegistry::getBlockData(block);
				if (!blockData || blockData->absorbsLight)
					break;

				lightLevels[belowIndex].skyLight = 15;
				neighborDirtyMask |= getNeighborDirtyMask(x, ny, z);

				for (int i = 0; i < horizontalDirections.size(); i++)
				{
					const glm::ivec2 nXZ = nXZs[i];
					const int nx = nXZ.x;
					const int nz = nXZ.y;
					tryPropagate(nx, ny, nz, 14, false);
				}
			}

			// Carry max light into the chunk below
			if (ny < 0)
			{
				Chunk* belowChunk = neighbors[getSideNeighborIndex(2)]; // -Y
				if (belowChunk)
				{
					constexpr int belowY = CHUNK_SIZE - 1;
					const size_t belowIndex = getIndex(x, belowY, z);

					if (belowChunk->lightLevels[belowIndex].skyLight < 15)
					{
						const auto* blockData = AssetRegistry::getBlockData(belowChunk->blocks[belowIndex]);
						if (blockData && !blockData->absorbsLight)
						{
							belowChunk->lightLevels[belowIndex].skyLight = 15;
							belowChunk->addSkyLightPropagationNode(x, belowY, z);
							neighborDirtyMask |= getNeighborDirtyMask(x, 0, z);
						}
					}
				}
			}
		}

		const int dirCount = isFullSunlight ? 4 : 6; // Skip vertical directions if source is full sunlight
		for (int i = 0; i < dirCount; i++)
		{
			const glm::ivec3 offset = DirectionsTable::directionsXZY[i];
			const int nx = x + offset.x;
			const int ny = y + offset.y;
			const int nz = z + offset.z;

			tryPropagate(nx, ny, nz, nextLight, true);
		}
	}

	return neighborDirtyMask;
}

uint32_t Chunk::propagateBlockLightRemoval()
{
	uint32_t neighborDirtyMask = 0;
	while (!LightPropagationStorage::threadLocalBlockLightRemoval.empty())
	{
		// Get node data
		const auto data = LightPropagationStorage::threadLocalBlockLightRemoval.pop_and_return_unsafe();

		// Propagate to neighbors
		for (int i = 0; i < 6; i++)
		{
			// Calculate neighbor block coordinates
			const glm::ivec3 offset = DirectionsTable::directionsXYZ[i];
			const int nx = data.x + offset.x;
			const int ny = data.y + offset.y;
			const int nz = data.z + offset.z;

			// Get neighbor chunk and block index
			size_t neighborBlockIndex;
			Chunk* neighborChunk;

			const bool isNeighborBlockInSameChunk = ((nx | ny | nz) & CHUNK_UPPER_BITS_MASK) == 0;
			if (isNeighborBlockInSameChunk)
			{
				neighborChunk = this;
				neighborBlockIndex = getIndex(nx, ny, nz);
			}
			else
			{
				neighborChunk = neighbors[getSideNeighborIndex(i)];
				if (!neighborChunk)
				{
					continue;
				}
				neighborBlockIndex = getIndex(nx & CHUNK_LOWER_BITS_MASK, ny & CHUNK_LOWER_BITS_MASK, nz & CHUNK_LOWER_BITS_MASK);
			}

			// Get neighbor block data
			BlockId neighborBlock = neighborChunk->getBlockAt(neighborBlockIndex);
			const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);

			// If block absorbs light, skip
			if (!neighborBlockData || neighborBlockData->absorbsLight)
			{
				continue;
			}

			// Propagate
			uint8_t neighborBlockLight = neighborChunk->getLightAt(neighborBlockIndex).blockLight;
			if (neighborBlockLight > 0 && neighborBlockLight < data.lightLevel)
			{
				neighborChunk->lightLevels[neighborBlockIndex].blockLight = 0;
				if (isNeighborBlockInSameChunk)
				{
					LightPropagationStorage::threadLocalBlockLightRemoval.emplace(nx, ny, nz, neighborBlockLight);
				}
				else
				{
					neighborChunk->addBlockLightRemovalNode(nx & CHUNK_LOWER_BITS_MASK, ny & CHUNK_LOWER_BITS_MASK, nz & CHUNK_LOWER_BITS_MASK, neighborBlockLight);
				}

				neighborDirtyMask |= getNeighborDirtyMask(nx, ny, nz);
			}
			else if (neighborBlockLight >= data.lightLevel)
			{
				if (isNeighborBlockInSameChunk)
				{
					LightPropagationStorage::threadLocalBlockLightPropagation.emplace(nx, ny, nz);
				}
				else
				{
					neighborChunk->addBlockLightPropagationNode(nx & CHUNK_LOWER_BITS_MASK, ny & CHUNK_LOWER_BITS_MASK, nz & CHUNK_LOWER_BITS_MASK);
				}
			}
		}
	}

	return neighborDirtyMask;
}

uint32_t Chunk::propagateSkyLightRemoval()
{
	uint32_t neighborDirtyMask = 0;
	while (!LightPropagationStorage::threadLocalSkyLightRemoval.empty())
	{
		// Get node data
		const auto data = LightPropagationStorage::threadLocalSkyLightRemoval.pop_and_return_unsafe();

		const bool isMaxLightLevel = data.lightLevel == 15;

		// Propagate to neighbors
		for (int i = 0; i < 6; i++)
		{
			// Calculate neighbor block coordinates
			const glm::ivec3 offset = DirectionsTable::directionsXYZ[i];
			const int nx = data.x + offset.x;
			const int ny = data.y + offset.y;
			const int nz = data.z + offset.z;

			// Get neighbor chunk and block index
			size_t neighborBlockIndex;
			Chunk* neighborChunk;

			const bool isNeighborBlockInSameChunk = ((nx | ny | nz) & CHUNK_UPPER_BITS_MASK) == 0;
			if (isNeighborBlockInSameChunk)
			{
				neighborChunk = this;
				neighborBlockIndex = getIndex(nx, ny, nz);
			}
			else
			{
				neighborChunk = neighbors[getSideNeighborIndex(i)];
				if (!neighborChunk)
				{
					continue;
				}
				neighborBlockIndex = getIndex(nx & CHUNK_LOWER_BITS_MASK, ny & CHUNK_LOWER_BITS_MASK, nz & CHUNK_LOWER_BITS_MASK);
			}

			// Get neighbor block data
			BlockId neighborBlock = neighborChunk->getBlockAt(neighborBlockIndex);
			const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);

			// If block absorbs light, skip
			if (!neighborBlockData || neighborBlockData->absorbsLight)
			{
				continue;
			}

			// Propagate
			uint8_t neighborSkyLight = neighborChunk->getLightAt(neighborBlockIndex).skyLight;
			if (neighborSkyLight > 0 &&
				(neighborSkyLight < data.lightLevel || (isMaxLightLevel && i == 2)))
			{
				neighborChunk->lightLevels[neighborBlockIndex].skyLight = 0;
				if (isNeighborBlockInSameChunk)
				{
					LightPropagationStorage::threadLocalSkyLightRemoval.emplace(nx, ny, nz, neighborSkyLight);
				}
				else
				{
					neighborChunk->addSkyLightRemovalNode(nx & CHUNK_LOWER_BITS_MASK, ny & CHUNK_LOWER_BITS_MASK, nz & CHUNK_LOWER_BITS_MASK, neighborSkyLight);
				}

				neighborDirtyMask |= getNeighborDirtyMask(nx, ny, nz);
			}
			else if (neighborSkyLight >= data.lightLevel)
			{
				if (isNeighborBlockInSameChunk)
				{
					LightPropagationStorage::threadLocalSkyLightPropagation.emplace(nx, ny, nz);
				}
				else
				{
					neighborChunk->addSkyLightPropagationNode(nx & CHUNK_LOWER_BITS_MASK, ny & CHUNK_LOWER_BITS_MASK, nz & CHUNK_LOWER_BITS_MASK);
				}
			}
		}
	}

	return neighborDirtyMask;
}

void Chunk::buildLight()
{
	if (!chunkFlags.read(Flag::IsLoadedInWorld))
	{
		return;
	}

	FenceGuard scopedFence(processingFence);

	PROFILE_SCOPE("Build chunk light", ProfileCategory::ChunkLight);

	const Chunk* topNeighbor = neighbors[getNeighborIndex(0, 1, 0)];

	// Collect block light sources
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

				lightLevels[index].blockLight = emission;
				LightPropagationStorage::threadLocalBlockLightPropagation.emplace(x, y, z);
			}
		}
	}

	// Collect sky light sources
	if (!topNeighbor)
	{
		// Create local heightmap for this chunk
		std::array<int, CHUNK_AREA> heightMap;
		heightMap.fill(-1);
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				for (int y = CHUNK_SIZE - 1; y >= 0; y--)
				{
					BlockId block = blocks[getIndex(x, y, z)];
					const auto* blockData = AssetRegistry::getBlockData(block);
					if (!blockData || blockData->absorbsLight)
					{
						heightMap[getIndex(x, z)] = y;
						break;
					}
				}
			}
		}
		
		// Compute array of heights from where to start adding nodes
		constexpr int MAX_LOCAL_HEIGHT = CHUNK_SIZE - 1;
		std::array<int, CHUNK_AREA> addNodeHeightMap;
		addNodeHeightMap.fill(MAX_LOCAL_HEIGHT); // Unfilled values will make nodes appear on every y coord
		// Coords won't be on border, so values on borders won't change
		for (int x = 1; x < CHUNK_SIZE - 1; x++)
		{
			for (int z = 1; z < CHUNK_SIZE - 1; z++)
			{
				// Get neighbor heights (in index increasing order)
				const int nxHeight = heightMap[getIndex(x - 1, z)];
				const int nzHeight = heightMap[getIndex(x, z - 1)];
				const int pzHeight = heightMap[getIndex(x, z + 1)];
				const int pxHeight = heightMap[getIndex(x + 1, z)];
		
				// Get max height
				const int maxXHeight = std::max(nxHeight, pxHeight);
				const int maxZHeight = std::max(pzHeight, nzHeight);
				const int maxHeight = std::max(maxXHeight, maxZHeight);
		
				// Set localHeightToStartAddingNodes to the corresponding max height minus one (there can be nothing under toppest block)
				int localHeightToStartAddingNodes = maxHeight - 1;
				localHeightToStartAddingNodes = std::max(localHeightToStartAddingNodes, 0); // Must place at bottom, so it can propagate on neighbor chunk
		
				addNodeHeightMap[getIndex(x, z)] = localHeightToStartAddingNodes;
			}
		}
		
		// Create nodes and fill light levels
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				const size_t index = getIndex(x, z);
				const int firstExposedY = heightMap[index] + 1;
		
				if (firstExposedY >= CHUNK_SIZE)
				{
					continue; // No exposed blocks in this column
				}
		
				// Filling light

				const int localHeightToStartAddingNodes = addNodeHeightMap[index];

				for (int y = localHeightToStartAddingNodes; y < CHUNK_SIZE; y++)
				{
					lightLevels[getIndex(x, y, z)].skyLight = 15;
				}

				LightPropagationStorage::threadLocalSkyLightPropagation.emplace(x, localHeightToStartAddingNodes, z);
			}
		}

		// // This code above can be simplified to this, but for some reason it results in slower execution
		// // Maybe because overhead of running propagateSkyLight
		//for (int x = 0; x < CHUNK_SIZE; x++)
		//{
		//	for (int z = 0; z < CHUNK_SIZE; z++)
		//	{
		//		size_t index = getIndex(x, CHUNK_SIZE - 1, z);
		//		BlockId block = blocks[index];
		//		const auto* blockData = AssetRegistry::getBlockData(block);
		//		if (!blockData || blockData->absorbsLight)
		//		{
		//			continue;
		//		}
		//
		//		lightLevels[index].skyLight = 15;
		//		LightPropagationStorage::threadLocalSkyLightPropagation.queue.emplace(x, CHUNK_SIZE - 1, z);
		//	}
		//}
	}

	// Collect light from neighbors
	// TODO: If neighbor block is solid, then check if it's a light source and propagate from it
	{
		auto processNeighborFace = [&](int x, int y, int z, int nx, int ny, int nz, const Chunk* neighbor, bool propagatingFromTop)
			{
				size_t index = getIndex(x, y, z);
				const auto* blockData = AssetRegistry::getBlockData(blocks[index]);
				if (!blockData || blockData->absorbsLight)
				{
					return;
				}

				LightLevel& currentLight = lightLevels[index];
				LightLevel neighborLight = neighbor->getLightAt(nx, ny, nz);

				// Block light
				if (currentLight.blockLight + 1 < neighborLight.blockLight)
				{
					currentLight.blockLight = neighborLight.blockLight - 1;
					LightPropagationStorage::threadLocalBlockLightPropagation.emplace(x, y, z);
				}

				// Sky light
				uint8_t skyLightAbsorption = (propagatingFromTop && neighborLight.skyLight == 15) ? 0 : 1;
				if (currentLight.skyLight + skyLightAbsorption < neighborLight.skyLight)
				{
					currentLight.skyLight = neighborLight.skyLight - skyLightAbsorption;
					LightPropagationStorage::threadLocalSkyLightPropagation.emplace(x, y, z);
				}
			};

		// -X
		const Chunk* neighbor = neighbors[getNeighborIndex(-1, 0, 0)];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int x = 0;
			const int neighborX = CHUNK_SIZE - 1;
			for (int y = 0; y < CHUNK_SIZE; y++)
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				processNeighborFace(x, y, z, neighborX, y, z, neighbor, false);
			}
		}

		// +X
		neighbor = neighbors[getNeighborIndex(1, 0, 0)];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int x = CHUNK_SIZE - 1;
			const int neighborX = 0;
			for (int y = 0; y < CHUNK_SIZE; y++)
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				processNeighborFace(x, y, z, neighborX, y, z, neighbor, false);
			}
		}

		// -Y
		neighbor = neighbors[getNeighborIndex(0, -1, 0)];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int y = 0;
			const int neighborY = CHUNK_SIZE - 1;
			for (int x = 0; x < CHUNK_SIZE; x++)
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				processNeighborFace(x, y, z, x, neighborY, z, neighbor, false);
			}
		}

		// +Y
		neighbor = topNeighbor;
		if (neighbor && neighbor->isLightBuilt())
		{
			const int y = CHUNK_SIZE - 1;
			const int neighborY = 0;
			for (int x = 0; x < CHUNK_SIZE; x++)
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				processNeighborFace(x, y, z, x, neighborY, z, neighbor, true);
			}
		}

		// -Z
		neighbor = neighbors[getNeighborIndex(0, 0, -1)];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int z = 0;
			const int neighborZ = CHUNK_SIZE - 1;
			for (int x = 0; x < CHUNK_SIZE; x++)
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				processNeighborFace(x, y, z, x, y, neighborZ, neighbor, false);
			}
		}

		// +Z
		neighbor = neighbors[getNeighborIndex(0, 0, 1)];
		if (neighbor && neighbor->isLightBuilt())
		{
			const int z = CHUNK_SIZE - 1;
			const int neighborZ = 0;
			for (int x = 0; x < CHUNK_SIZE; x++)
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				processNeighborFace(x, y, z, x, y, neighborZ, neighbor, false);
			}
		}
	}

	// Propagate light
	propagateBlockLight();
	propagateSkyLight();

	// Set state
	setState(State::LightsBuilt);

	// Apply dirty mask
	//applyNeighborDirtyMask(neighborDirtyMask);
	applyNeighborDirtyMask(-1); // Update all neighbors, because 'buildLight' is called after 'buildBlocks', so every neighbor is dirty

	// Let neighbor chunks' lights be updated
	bool hasNeighborToUpdate = false;
	for (int i = 0; i < 6; i++)
	{
		auto index = getSideNeighborIndex(i);
		Chunk* neighbor = neighbors[index];
		if (neighbor && neighbor->lightPropagation.hasNodes())
		{
			neighbor->parentRegion->setFlag(ChunkRegion::Flag::HasLightToUpdate, true);
			hasNeighborToUpdate = true;
		}
	}
	if (hasNeighborToUpdate)
	{
		ChunkRegion::setGlobalFlag(ChunkRegion::Flag::HasLightToUpdate, true);
	}
}

void Chunk::updateLight()
{
	if (!chunkFlags.read(Flag::IsLoadedInWorld))
	{
		return;
	}

	PROFILE_SCOPE("Update chunk light", ProfileCategory::ChunkLight);

	FenceGuard scopedFence(processingFence);

	lightPropagation.swapQueuesWithLocal();

	uint32_t neighborDirtyMask = 0;

	// Remove block light
	neighborDirtyMask |= propagateBlockLightRemoval();

	// Propagate block light
	neighborDirtyMask |= propagateBlockLight();

	// Remove sky light
	neighborDirtyMask |= propagateSkyLightRemoval();

	// Propagate sky light
	neighborDirtyMask |= propagateSkyLight();

	// Apply dirty mask
	applyNeighborDirtyMask(neighborDirtyMask);

	// Let neighbor chunks' lights be updated
	bool hasNeighborToUpdate = false;
	for (int i = 0; i < 6; i++)
	{
		auto index = getSideNeighborIndex(i);
		Chunk* neighbor = neighbors[index];
		if (neighbor && neighbor->lightPropagation.hasNodes())
		{
			neighbor->parentRegion->setFlag(ChunkRegion::Flag::HasLightToUpdate, true);
			hasNeighborToUpdate = true;
		}
	}
	if (hasNeighborToUpdate)
	{
		ChunkRegion::setGlobalFlag(ChunkRegion::Flag::HasLightToUpdate, true);
	}
}

void Chunk::updateMesh()
{
	if (!chunkFlags.read(Flag::IsLoadedInWorld))
	{
		return;
	}
	
	FenceGuard scopedFence(processingFence);

	PROFILE_SCOPE("Update chunk mesh", ProfileCategory::ChunkMesh);

	// Collect visible faces
	{
		const uint32_t transformationBitMasks[3] = { 0u, 0b11u, 0b111u };

		static const BlockData::TextureSlot fallbackTextureSlot(0, BlockData::TextureSlot::TextureTransformation::None, false);

		localMeshInstances.clear();

		const int globalChunkX = position.x * CHUNK_SIZE;
		const int globalChunkY = position.y * CHUNK_SIZE;
		const int globalChunkZ = position.z * CHUNK_SIZE;
		for (size_t currentBlockIndex = 0; currentBlockIndex < CHUNK_VOLUME; currentBlockIndex++)
		{
			glm::ivec3 currentBlockPosition = getPositionFromIndex(currentBlockIndex);

			// Generate new faces for this block
			BlockId block = blocks[currentBlockIndex];
			const BlockData* blockData = AssetRegistry::getBlockData(block);
			if (!(blockData && blockData->hasFaces))
			{
				continue;
			}

			const auto* model = AssetRegistry::getBlockModelData(blockData->modelId);
			if (!model)
			{
				continue;
			}

			const auto& textureSlots = blockData->textureSlots;

			// TODO: Maybe translucent faces shouldn't be culled. Maybe they should be drawn using GL_LEQUAL for depth test.

			// Aligned faces
			if (!model->alignedFaces.empty())
			{
				const uint32_t hash = hash3(
					globalChunkX + currentBlockPosition.x,
					globalChunkY + currentBlockPosition.y,
					globalChunkZ + currentBlockPosition.z
				);
				for (const auto& face : model->alignedFaces)
				{
					// Get neighbor block coordinates
					const glm::ivec3 offset = DirectionsTable::directionsXYZ[face.normal];
					const int nx = currentBlockPosition.x + offset.x;
					const int ny = currentBlockPosition.y + offset.y;
					const int nz = currentBlockPosition.z + offset.z;

					// Get neighbor chunk and block index
					size_t neighborBlockIndex;
					const Chunk* neighborChunk;

					const bool inSameChunk = ((nx | ny | nz) & CHUNK_UPPER_BITS_MASK) == 0;

					// TODO: Try to remove this branch
					if (inSameChunk)
					{
						neighborChunk = this;
						neighborBlockIndex = getIndex(nx, ny, nz);
					}
					else
					{
						neighborChunk = neighbors[getSideNeighborIndex(face.normal)];
						if (!neighborChunk)
						{
							continue;
						}
						neighborBlockIndex = getIndex(nx & CHUNK_LOWER_BITS_MASK, ny & CHUNK_LOWER_BITS_MASK, nz & CHUNK_LOWER_BITS_MASK);
					}
					
					// Get neighbor block and data
					BlockId neighborBlock = neighborChunk->blocks[neighborBlockIndex];
					if (block == neighborBlock)
					{
						continue;
					}

					const BlockData* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
					if (!neighborBlockData || neighborBlockData->faceCulling[face.normal ^ 1])
					{
						continue;
					}

					// Calculate shading
					LightLevel neighborLight = neighborChunk->lightLevels[neighborBlockIndex];
					
					ContextFaceAOAL aoData
					{
						.x = currentBlockPosition.x,
						.y = currentBlockPosition.y,
						.z = currentBlockPosition.z,
						.normal = face.normal,
						.centerFaceLight = neighborLight
					};

					calculateFaceAmbientOcclusionAndLight(aoData);

					// Get texture
					const auto& textureSlot = face.textureSlot < textureSlots.size() ? textureSlots[face.textureSlot] : fallbackTextureSlot;

					// Calculate texture transformation
					uint32_t faceTransformation = hash & transformationBitMasks[(size_t)textureSlot.transformation];

					// Add new face
					auto& instances = textureSlot.isTranslucent ? localMeshInstances.alignedTranslucent : localMeshInstances.alignedOpaque;
					instances.emplace_back(
						currentBlockPosition.x, currentBlockPosition.y, currentBlockPosition.z,
						face.normal,
						aoData.outAmbientOcclusion,
						textureSlot.textureId,
						faceTransformation,
						aoData.outLightLevel
					);
				}
			}

			// Non-aligned faces
			// TODO: Non-aligned faces should be culled if they are on the block's border
			if (!model->unalignedFaces.empty())
			{
				BlockVertexLightData lightData;
				calculateBlockVertexLight(lightData, currentBlockPosition.x, currentBlockPosition.y, currentBlockPosition.z);

				UnalignedBlockFace instance;
				instance.blockX = currentBlockPosition.x;
				instance.blockY = currentBlockPosition.y;
				instance.blockZ = currentBlockPosition.z;

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

				for (const auto& face : model->unalignedFaces)
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

					auto& instances = textureSlot.isTranslucent ? localMeshInstances.unalignedTranslucent : localMeshInstances.unalignedOpaque;

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
		FenceGuard scopedMeshFence(mesh.faceStorage.processingFence);

		mesh.faceStorage.instancesStorage.swap(localMeshInstances);

		size_t faceCount = mesh.faceStorage.getAllFaceCount();
		if (faceCount == 0)
		{
			mesh.setFlag(ChunkMesh::Flag::ShouldBeUploaded, false);

			mesh.faceStorage.updateRenderFaceCount();

			updateCanBeRenderedFlag();
		}
		else
		{
			mesh.setFlag(ChunkMesh::Flag::ShouldBeUploaded, true);

			// Region flag
			parentRegion->setFlag(ChunkRegion::Flag::HasMeshToUpload, true);
			ChunkRegion::setGlobalFlag(ChunkRegion::Flag::HasMeshToUpload, true);
		}
	}
}

void Chunk::markMeshDirty()
{
	// Self flag
	setFlag(Flag::ShouldUpdateMesh, true);

	// Region flag
	parentRegion->setFlag(ChunkRegion::Flag::HasMeshToUpdate, true);
	ChunkRegion::setGlobalFlag(ChunkRegion::Flag::HasMeshToUpdate, true);
}

void Chunk::askForMeshUpload()
{
	if (mesh.readFlag(ChunkMesh::Flag::ShouldBeUploaded))
	{
		mesh.pendingMeshUploads.push_back(this);
	}
}

void Chunk::updateCanBeRenderedFlag() noexcept
{
	bool newValue = mesh.faceStorage.getAllRenderFaceCount() > 0;
	bool oldValue = readAndSetFlag(Flag::CanBeRendered, newValue);

	if (newValue && !oldValue)
	// Appeared
	{
		parentRegion->incrementRenderChunkCount();
	}
	// Disappeared
	else if (!newValue && oldValue)
	{
		parentRegion->decrementRenderChunkCount();
	}
}

Chunk* Chunk::traverseToSideNeighbor(int x, int y, int z, int side, size_t& outIndex) const
{
	if (((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0)
	{
		outIndex = getIndex(x, y, z);
		return const_cast<Chunk*>(this);
	}
	
	Chunk* neighbor = neighbors[getSideNeighborIndex(side)];
	if (!neighbor) return nullptr;

	outIndex = getIndex(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
	return neighbor;
}

Chunk* Chunk::traverseThroughNeighbors(int x, int y, int z, size_t& outIndex) const
{
	if (((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0)
	{
		outIndex = getIndex(x, y, z);
		return const_cast<Chunk*>(this);
	}

	const int dirX = (x >= CHUNK_SIZE) - (x < 0);
	const int dirY = (y >= CHUNK_SIZE) - (y < 0);
	const int dirZ = (z >= CHUNK_SIZE) - (z < 0);

	Chunk* neighbor = neighbors[getNeighborIndex(dirX, dirY, dirZ)];
	if (!neighbor) return nullptr;

	outIndex = getIndex(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
	return neighbor;
}

uint32_t Chunk::getNeighborDirtyMask(int x, int y, int z) noexcept
{
	// Convert block coordinates to offset flags (-1, 0, or 1)
	const int dx = (x <= 0) ? -1 : (x >= (CHUNK_SIZE - 1)) ? 1 : 0;
	const int dy = (y <= 0) ? -1 : (y >= (CHUNK_SIZE - 1)) ? 1 : 0;
	const int dz = (z <= 0) ? -1 : (z >= (CHUNK_SIZE - 1)) ? 1 : 0;

	// Index the LUT directly with these offsets
	int lutIndex = getNeighborIndex(dx, dy, dz);
	return PRECOMPUTED_NEIGHBOR_DIRTY_MASKS[lutIndex];
}

void Chunk::applyNeighborDirtyMask(uint32_t mask)
{
	// Using a loop with bit manipulation because it should reduce the number of branching and memory fetches

	constexpr size_t neighborCount = sizeof(neighbors) / sizeof(neighbors[0]); // Idk, neighbor.size() doesn't work for some reason
	constexpr uint32_t clearTrashMask = (1u << neighborCount) - 1; // Mask to clear bits that are out of bounds
	mask &= clearTrashMask;

	while (mask)
	{
		int i = std::countr_zero(mask); // Get index of lowest set bit
		if (neighbors[i]) neighbors[i]->markMeshDirty();
		mask &= mask - 1; // Clear lowest set bit
	}
}

std::pair<BlockId, LightLevel> Chunk::getBlockAndLightAt(int x, int y, int z) const
{
	size_t index = getIndex(x, y, z);
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
		removeBlockChange(previousBlock, static_cast<uint16_t>(index));
		blockChanges[block].push_back(static_cast<uint16_t>(index));
	}

	// Light update
	const BlockData* previousBlockData = AssetRegistry::getBlockDataSafe(previousBlock);
	uint8_t previousEmission = previousBlockData->lightEmission;

	const BlockData* newBlockData = AssetRegistry::getBlockDataSafe(block);
	uint8_t newEmission = newBlockData->lightEmission;

	if (previousBlockData->absorbsLight && !newBlockData->absorbsLight)
	{
		// Collect maximum light level from neighbors and propagate it to this block
		uint8_t maxBlockLightToSet = 0;
		uint8_t maxSkyLightToSet = 0;
		for (int i = 0; i < 6; i++)
		{
			int nx = x + DirectionsTable::dx[i];
			int ny = y + DirectionsTable::dy[i];
			int nz = z + DirectionsTable::dz[i];

			size_t neighborIndex;
			Chunk* neighborChunk = traverseToSideNeighbor(nx, ny, nz, i, neighborIndex);
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
			if (maxBlockLightToSet > 1) addBlockLightPropagationNode(x, y, z);
		}
		if (maxSkyLightToSet > 0)
		{
			lightLevels[index].skyLight = maxSkyLightToSet;
			if (maxSkyLightToSet > 1) addSkyLightPropagationNode(x, y, z);
		}
	}
	else if (!previousBlockData->absorbsLight && newBlockData->absorbsLight)
	{
		// Remove light at this block
		uint8_t currentBlockLight = lightLevels[index].blockLight;
		if (currentBlockLight > 0)
		{
			lightLevels[index].blockLight = 0;
			addBlockLightRemovalNode(x, y, z, currentBlockLight);
		}
		uint8_t currentSkyLight = lightLevels[index].skyLight;
		if (currentSkyLight > 0)
		{
			lightLevels[index].skyLight = 0;
			addSkyLightRemovalNode(x, y, z, currentSkyLight);
		}
	}

	// Handle light emission changes
	if (previousEmission != newEmission)
	{
		if (previousEmission > newEmission)
		{
			lightLevels[index].blockLight = 0;
			addBlockLightRemovalNode(x, y, z, previousEmission);
		}

		if (newEmission > 0)
		{
			lightLevels[index].blockLight = newEmission;
			addBlockLightPropagationNode(x, y, z);
		}
	}

	// Mark meshes as dirty
	markMeshesDirtyAroundBlock(x, y, z);
	
	// Allow update light
	if (lightPropagation.hasNodes())
	{
		parentRegion->setFlag(ChunkRegion::Flag::HasLightToUpdate, true);
		ChunkRegion::setGlobalFlag(ChunkRegion::Flag::HasLightToUpdate, true);
	}
}

void Chunk::setLightAt(int x, int y, int z, LightLevel lightValue)
{
	lightLevels[getIndex(x, y, z)] = lightValue;
	markMeshesDirtyAroundBlock(x, y, z);
}

void Chunk::setBlockLightAt(int x, int y, int z, uint8_t lightLevel)
{
	lightLevels[getIndex(x, y, z)].blockLight = lightLevel;
	markMeshesDirtyAroundBlock(x, y, z);
}

void Chunk::setSkyLightAt(int x, int y, int z, uint8_t lightLevel)
{
	lightLevels[getIndex(x, y, z)].skyLight = lightLevel;
	markMeshesDirtyAroundBlock(x, y, z);
}

void Chunk::setLightAt(size_t index, LightLevel lightValue)
{
	lightLevels[index] = lightValue;

	glm::ivec3 pos = getPositionFromIndex(index);
	markMeshesDirtyAroundBlock(pos.x, pos.y, pos.z);
}

void Chunk::setBlockLightAt(size_t index, uint8_t lightLevel)
{
	lightLevels[index].blockLight = lightLevel;

	glm::ivec3 pos = getPositionFromIndex(index);
	markMeshesDirtyAroundBlock(pos.x, pos.y, pos.z);
}

void Chunk::setSkyLightAt(size_t index, uint8_t lightLevel)
{
	lightLevels[index].skyLight = lightLevel;

	glm::ivec3 pos = getPositionFromIndex(index);
	markMeshesDirtyAroundBlock(pos.x, pos.y, pos.z);
}

void Chunk::addBlockLightPropagationNode(int x, int y, int z)
{
	FenceGuard scopedFence(lightPropagation.blockLightPropagationProcessingFence);
	lightPropagation.blockLightPropagationQueue.emplace(x, y, z);
}

void Chunk::addBlockLightRemovalNode(int x, int y, int z, uint8_t lightLevel)
{
	FenceGuard scopedFence(lightPropagation.blockLightRemovalProcessingFence);
	lightPropagation.blockLightRemovalQueue.emplace(x, y, z, lightLevel);
}

void Chunk::addSkyLightPropagationNode(int x, int y, int z)
{
	FenceGuard scopedFence(lightPropagation.skyLightPropagationProcessingFence);
	lightPropagation.skyLightPropagationQueue.emplace(x, y, z);
}

void Chunk::addSkyLightRemovalNode(int x, int y, int z, uint8_t lightLevel)
{
	FenceGuard scopedFence(lightPropagation.skyLightRemovalProcessingFence);
	lightPropagation.skyLightRemovalQueue.emplace(x, y, z, lightLevel);
}

void Chunk::markMeshesDirtyAroundBlock(int x, int y, int z)
{
	uint32_t dirtyMask = getNeighborDirtyMask(x, y, z);
	applyNeighborDirtyMask(dirtyMask);
}

constexpr unsigned int getMagicNumberForDivision(unsigned int divisor, unsigned int precision)
{
	return (1u << precision) / divisor + 1;
}

void Chunk::calculateVertexAmbientOcclusionAndLight(
	unsigned int& ao,
	LightLevel& light,
	const LightLevel& centerLight,
	const LightLevelAndIsSolid& side1,
	const LightLevelAndIsSolid& side2,
	const LightLevelAndIsSolid& corner
) const
{
	// Tried to pass 'LightLevelAndIsSolid' arguments by value, but it made the code slower, even if they are just 2 bytes each
	// Passing even one byte by reference make everything faster, probably compiler stuff

	// Division fuckery
	constexpr std::array<unsigned int, 4> magicNumbers = {
		getMagicNumberForDivision(1, 8),
		getMagicNumberForDivision(2, 8),
		getMagicNumberForDivision(3, 8),
		getMagicNumberForDivision(4, 8)
	};

	// Main thingy
	const unsigned bothSolid = side1.isSolid & side2.isSolid;
	const unsigned use1 = !side1.isSolid;
	const unsigned use2 = !side2.isSolid;
	const unsigned useC = !corner.isSolid & !bothSolid;

	const unsigned blockLightSum =
		centerLight.blockLight +
		use1 * side1.lightLevel.blockLight +
		use2 * side2.lightLevel.blockLight +
		useC * corner.lightLevel.blockLight;

	const unsigned skyLightSum =
		centerLight.skyLight +
		use1 * side1.lightLevel.skyLight +
		use2 * side2.lightLevel.skyLight +
		useC * corner.lightLevel.skyLight;

	const unsigned count = use1 + use2 + useC;
	const unsigned int magicNumber = magicNumbers[count];
	light.blockLight = (blockLightSum * magicNumber) >> 8u;
	light.skyLight = (skyLightSum * magicNumber) >> 8u;
	ao = (1u - bothSolid) * count;
}

void Chunk::calculateFaceAmbientOcclusionAndLight(ContextFaceAOAL& context) const
{
	// For each face normal, we need to check 8 neighbors around the face
	// The AO calculation depends on which direction the face is facing

	LightLevelAndIsSolid neighborData[8];

	unsigned int ao0 = 0, ao1 = 0, ao2 = 0, ao3 = 0;
	LightLevel lightLevels[4];

	auto x = context.x;
	auto y = context.y;
	auto z = context.z;
	auto normal = context.normal;
	auto centerFaceLight = context.centerFaceLight;


	auto getSafe = [this, &neighborData, normal](size_t dataIdx, int x_, int y_, int z_)
		{
			size_t index;
			const Chunk* chunk = traverseThroughNeighbors(x_, y_, z_, index);

			//neighborData[dataIdx].isSolid = true; // Already solid by default
			if (!chunk) return;

			BlockId block = chunk->blocks[index];
			LightLevel lightLevel = chunk->lightLevels[index];

			neighborData[dataIdx].lightLevel = lightLevel;

			const auto* blockData = AssetRegistry::getBlockData(block);
			neighborData[dataIdx].isSolid = blockData && blockData->faceCulling[normal ^ 1];
		};

	switch (normal)
	{
	case 0: // -X face
		x--;
		getSafe(0, x, y - 1, z - 1);
		getSafe(1, x, y - 1, z    );
		getSafe(2, x, y - 1, z + 1);
		getSafe(3, x, y    , z - 1);
		getSafe(4, x, y    , z + 1);
		getSafe(5, x, y + 1, z - 1);
		getSafe(6, x, y + 1, z)    ;
		getSafe(7, x, y + 1, z + 1);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		break;
	case 1: // +X face
		x++;
		getSafe(0, x, y - 1, z - 1);
		getSafe(1, x, y - 1, z    );
		getSafe(2, x, y - 1, z + 1);
		getSafe(3, x, y    , z - 1);
		getSafe(4, x, y    , z + 1);
		getSafe(5, x, y + 1, z - 1);
		getSafe(6, x, y + 1, z    );
		getSafe(7, x, y + 1, z + 1);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 2: // -Y face
		y--;
		getSafe(0, x - 1, y, z - 1);
		getSafe(1, x    , y, z - 1);
		getSafe(2, x + 1, y, z - 1);
		getSafe(3, x - 1, y, z    );
		getSafe(4, x + 1, y, z    );
		getSafe(5, x - 1, y, z + 1);
		getSafe(6, x    , y, z + 1);
		getSafe(7, x + 1, y, z + 1);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 3: // +Y face
		y++;
		getSafe(0, x - 1, y, z - 1);
		getSafe(1, x    , y, z - 1);
		getSafe(2, x + 1, y, z - 1);
		getSafe(3, x - 1, y, z    );
		getSafe(4, x + 1, y, z    );
		getSafe(5, x - 1, y, z + 1);
		getSafe(6, x    , y, z + 1);
		getSafe(7, x + 1, y, z + 1);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[4], neighborData[1], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[3], neighborData[1], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[3], neighborData[6], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[4], neighborData[6], neighborData[7]);
		break;
	case 4: // -Z face
		z--;
		getSafe(0, x - 1, y - 1, z);
		getSafe(1, x    , y - 1, z);
		getSafe(2, x + 1, y - 1, z);
		getSafe(3, x - 1, y    , z);
		getSafe(4, x + 1, y    , z);
		getSafe(5, x - 1, y + 1, z);
		getSafe(6, x    , y + 1, z);
		getSafe(7, x + 1, y + 1, z);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 5: // +Z face
		z++;
		getSafe(0, x - 1, y - 1, z);
		getSafe(1, x    , y - 1, z);
		getSafe(2, x + 1, y - 1, z);
		getSafe(3, x - 1, y    , z);
		getSafe(4, x + 1, y    , z);
		getSafe(5, x - 1, y + 1, z);
		getSafe(6, x    , y + 1, z);
		getSafe(7, x + 1, y + 1, z);

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		break;
	}

	context.outAmbientOcclusion = ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6);

	auto& light = context.outLightLevel;

	static_assert(sizeof(light) >= sizeof(lightLevels), "light packing too small");
	std::memcpy(&light, lightLevels, sizeof(light));
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
