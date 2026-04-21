#include "Chunk.h"

#include "TerrainGenerator.h"

#include "Game/DataPackManagment/AssetRegistry.h"
#include "Game/TracyProfiler.h"

#include "Core/Assert.h"
#include "Core/Hashes/ivec2Hasher.h"

std::unique_ptr<Chunk::ManagerInstances> Chunk::managerInstances;

Chunk::CachedBlockIds Chunk::CACHED_BLOCK_IDS;


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

static uint32_t hash3(const glm::ivec3& p)
{
	uint32_t data = (uint32_t)p.x * 0x27d4eb2du + (uint32_t)p.y * 0x165667b1u + (uint32_t)p.z * 0x1b873593u;
	data ^= data >> 15u;
	data *= 0x85ebca6bu;
	data ^= data >> 13u;
	data *= 0xc2b2ae35u;
	data ^= data >> 16u;
	return data;
}

// Prepares chunk for use
void Chunk::init(const glm::ivec3& newPosition, const std::array<Chunk*, 27>& newNeighbors, ChunkRegion* newParentRegion)
{
	TRACY_SCOPE_NC("Chunk init", ProfileCategory::ChunkLoadUnload);

	// Set position and parent region
	position = newPosition;
	parentRegion = newParentRegion;

	// Set neighbors
	neighbors = newNeighbors;
	constexpr int selfIndex = getNeighborIndex(0, 0, 0);

	for (int i = 0; i < selfIndex; i++)
	{
		Chunk* neighbor = neighbors[i];
		if (neighbor)
		{
			neighbor->neighbors[getOppositeNeighborIndex(i)] = this;
		}
	}
	neighbors[selfIndex] = this;
	for (int i = selfIndex + 1; i < newNeighbors.size(); i++)
	{
		Chunk* neighbor = neighbors[i];
		if (neighbor)
		{
			neighbor->neighbors[getOppositeNeighborIndex(i)] = this;
		}
	}

	// Reset state and flags
	setState(Chunk::State::NotInitialized_NeedsBlocks);

	chunkFlags.reset();
	chunkFlags.set(Flag::IsLoadedInWorld, true);

	// Reset mesh data
	mesh.faceStorage.resetRenderFaceCount();
	mesh.setFlag(ChunkMesh::Flag::ShouldBeUploaded, false);
}

// Cleans up resources
void Chunk::destroy()
{
	TRACY_SCOPE_NC("Chunk destroy", ProfileCategory::ChunkLoadUnload);

	// Release chunk column data
	if (chunkFlags.readAndSet(Flag::IsLoadedChunkColumnData, false))
	{
		TerrainGenerator::getInstance().unloadChunkColumnData(position.x, position.z);
	}

	// Set states and flags
	setFlag(Flag::IsLoadedInWorld, false);
	setState(Chunk::State::NotInitialized_NeedsBlocks);

	// Reset mesh data
	if (readFlag(Flag::CanBeRendered))
	{
		parentRegion->decrementRenderChunkCount();
	}

	{
		FenceGuard scopedFence(mesh.faceStorage.processingFence);
		mesh.faceStorage.instancesStorage.clear();
		mesh.faceStorage.instancesStorage.shrinkToFit();
	}
	
	// Reset neighbors
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
	{
		FenceGuard fence(processingFence);
		lightPropagation.clear();
	}

	// TODO: Make it async. Mark chunk as processing.
	save();
	blockChanges.clear();
}

void Chunk::globalInit()
{
	// Create manager instances (using for clear lifetime)
	managerInstances = std::make_unique<ManagerInstances>();

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
	// Destroy manager instances
	managerInstances.reset();
}

void Chunk::buildBlocks()
{
	if (!chunkFlags.read(Flag::IsLoadedInWorld))
	{
		return;
	}

	TRACY_SCOPE_NC("Build chunk blocks", ProfileCategory::ChunkBlocks);

	FenceGuard scopedFence(processingFence);

	// Reset blocks
	{
		TRACY_SCOPE_NC("Reset blocks", ProfileCategory::General);
		//std::memset(blocks, CACHED_BLOCK_IDS.airId, sizeof(blocks));
		
		for (int i = 0; i < CHUNK_VOLUME; i++)
		{
			cells[i].block = CACHED_BLOCK_IDS.airId;
		}
	}

	// Load chunk column data
	const ChunkColumnData* chunkColumnData;
	chunkColumnData = TerrainGenerator::getInstance().loadChunkColumnData(position.x, position.z);
	chunkFlags.set(Flag::IsLoadedChunkColumnData, true);
	const int* heightMap = chunkColumnData->heightMapRead();

	// Terrain
	bool computeCaveMask = false;

	constexpr int OCEAN_LEVEL = 0;

	const glm::ivec3 globalChunkPosition = position * CHUNK_SIZE;

	const bool isInTerrainRange = globalChunkPosition.y <= chunkColumnData->getMaxHeight();
	const bool isInWaterRange = globalChunkPosition.y <= OCEAN_LEVEL;

	if (isInTerrainRange || isInWaterRange)
	{
		{
			TRACY_SCOPE_NC("Build terrain", ProfileCategory::ChunkBlocks);

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
						cells[index].block = CACHED_BLOCK_IDS.stoneId;
						index += CoordinatesStride3D::y;
					}

					// Dirt
					computeCaveMask |= dirtEnd > dirtStart;
					index = getIndex(x, dirtStart, z);
					for (int y = dirtStart; y < dirtEnd; y++)
					{
						cells[index].block = CACHED_BLOCK_IDS.dirtId;
						index += CoordinatesStride3D::y;
					}

					// Grass
					if (hasSurface)
					{
						index = getIndex(x, surfaceY, z);
						cells[index].block = CACHED_BLOCK_IDS.grassBlockId;
						computeCaveMask = true;
					}

					// Water
					computeCaveMask |= waterEnd > waterStart;
					index = getIndex(x, waterStart, z);
					for (int y = waterStart; y < waterEnd; y++)
					{
						cells[index].block = CACHED_BLOCK_IDS.waterId;
						index += CoordinatesStride3D::y;
					}

					// Air
					index = getIndex(x, airStart, z);
					for (int y = airStart; y < CHUNK_SIZE; y++)
					{
						cells[index].block = CACHED_BLOCK_IDS.airId;
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

			TRACY_SCOPE_NC("Generate caves", ProfileCategory::ChunkBlocks);

			for (int i = 0; i < CHUNK_VOLUME; i++)
			{
				if (caveMask[i])
				{
					cells[i].block = CACHED_BLOCK_IDS.airId;
				}
			}
		}
	}

	// Trees
	// TODO: Fix, trees spawning in air
	{
		TRACY_SCOPE_NC("Generate trees", ProfileCategory::ChunkBlocks);
	
		const ivec2Hasher hasher;

		const glm::ivec2 globalChunkXZ = { globalChunkPosition.x, globalChunkPosition.z };

		for (int x = 0; x < CHUNK_SIZE; x += 2)
		{
			for (int z = 0; z < CHUNK_SIZE; z += 2)
			{
				int treeRootHeight = heightMap[z + (x << CHUNK_SIZE_LOG2)];
				int localY = treeRootHeight - globalChunkPosition.y;

				//if ((localY | CHUNK_UPPER_BITS_MASK) != 0) // Allow only inside chunk
				//{
				//	continue;
				//}

				if (localY < 0 || localY > CHUNK_SIZE - 1) continue;

				size_t rootIndex = getIndex(x, localY, z);
				if (getBlockAt(rootIndex) != CACHED_BLOCK_IDS.grassBlockId)
				{
					continue;
				}

				glm::ivec2 worldPos = globalChunkXZ + glm::ivec2(x, z);

				size_t hashValue = hasher(worldPos);

				if ((hashValue % 100) >= 2)
				{
					continue;
				}

				cells[rootIndex].block = CACHED_BLOCK_IDS.dirtId;
				generateTree({ x, localY + 1, z });
			}
		}
	}

	// Incoming structures
	{
		TRACY_SCOPE_NC("Apply incoming structural changes", ProfileCategory::ChunkBlocks);

		auto pendingChanges = managerInstances->structureBlock.retrieveAndClearChanges(position);
		for (const auto& change : pendingChanges)
		{
			if (!change.placeIfBlockIsAir || cells[change.index].block == CACHED_BLOCK_IDS.airId)
			{
				cells[change.index].block = change.block;
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
	if constexpr (USE_CONNECTIVITY_TESTING)
	{
		markAsShouldUpdateConnectivity();
	}
}

void Chunk::updateStructureBlocks()
{
	TRACY_SCOPE_NC("Update chunk structure blocks", ProfileCategory::ChunkBlocks);

	FenceGuard scopedFence(processingFence);

	auto pendingChanges = managerInstances->structureBlock.retrieveAndClearChanges(position);
	for (const auto& change : pendingChanges)
	{
		if (!change.placeIfBlockIsAir || cells[change.index].block == CACHED_BLOCK_IDS.airId)
		{
			auto pos = getPositionFromIndex(change.index);
			setBlockAt(pos.x, pos.y, pos.z, change.block, false);
		}
	}

	if constexpr (USE_CONNECTIVITY_TESTING)
	{
		if (!pendingChanges.empty())
		{
			markAsShouldUpdateConnectivity();
		}
	}
}

void Chunk::generateTree(const glm::ivec3& rootPosition)
{
	constexpr int treeHeight = 4;

	bool hasReachedOtherChunk = false;

	// Trunk
	for (int i = 0; i < treeHeight; i++)
	{
		int x = rootPosition.x;
		int y = rootPosition.y + i;
		int z = rootPosition.z;

		if (((x | y | z) & CHUNK_UPPER_BITS_MASK) == 0)
		{
			size_t index = getIndex(x, y, z);
			if (cells[index].block == CACHED_BLOCK_IDS.airId)
			{
				cells[index].block = CACHED_BLOCK_IDS.oakLogId;
			}
		}
		else
		{
			glm::ivec3 chunkPos = position;

			if (x < 0) chunkPos.x--;
			else if (x >= CHUNK_SIZE) chunkPos.x++;

			if (y < 0) chunkPos.y--;
			else if (y >= CHUNK_SIZE) chunkPos.y++;

			if (z < 0) chunkPos.z--;
			else if (z >= CHUNK_SIZE) chunkPos.z++;

			int nx = x & CHUNK_LOWER_BITS_MASK;
			int ny = y & CHUNK_LOWER_BITS_MASK;
			int nz = z & CHUNK_LOWER_BITS_MASK;
			size_t index = getIndex(nx, ny, nz);

			managerInstances->structureBlock.addChange(chunkPos, CACHED_BLOCK_IDS.oakLogId, index, true);
			hasReachedOtherChunk = true;
		}
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

				float squaredDistance = dx * dx + dz * dz + dy * dy * 0.8f; // Slightly elliptical

				if (squaredDistance > 4.0f)
				{
					continue;
				}

				if (((lx | ly | lz) & CHUNK_UPPER_BITS_MASK) == 0)
				{
					size_t index = getIndex(lx, ly, lz);
					if (cells[index].block == CACHED_BLOCK_IDS.airId)
					{
						cells[index].block = CACHED_BLOCK_IDS.oakLeavesId;
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

					managerInstances->structureBlock.addChange(chunkPos, CACHED_BLOCK_IDS.oakLeavesId, index, true);
					hasReachedOtherChunk = true;
				}
			}
		}
	}

	if (hasReachedOtherChunk)
	{
		managerInstances->structureBlock.hasAnyChanges.store(true, std::memory_order_release);
	}
}

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
	if (!parentRegion) return;

	// Skip the filesystem entirely if the region tells us this chunk has never been written to disk
	size_t indexInRegion = ChunkRegion::getChunkIndexInRegion(position);
	bool hasSavedData;
	{
		TRACY_SCOPE_NC("Check for saved data", ProfileCategory::ChunkBlocks);
		hasSavedData = parentRegion->hasSavedData(indexInRegion);
	}
	if (!hasSavedData) return;

	//ChunkIO::loadBlocks(blockChanges, blocks, parentRegion->getPosition(), indexInRegion);
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
	TRACY_SCOPE_NC("Propagate block light", ProfileCategory::ChunkLight);

	uint32_t neighborDirtyMask = 0;
	while (!LightPropagationStorage::threadLocalBlockLightPropagation.empty())
	{
		// Get node data
		const auto data = LightPropagationStorage::threadLocalBlockLightPropagation.pop_and_return_unsafe();

		// Get light level at current block
		uint8_t blockLight = cells[getIndex(data.x, data.y, data.z)].lightLevel.blockLight;
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
			neighborChunk->cells[neighborBlockIndex].lightLevel.blockLight = lightToSet;
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
	TRACY_SCOPE_NC("Propagate sky light", ProfileCategory::ChunkLight);

	constexpr std::array<int, 4> horizontalDirections{ 0, 1, 4, 5 };

	uint32_t neighborDirtyMask = 0;

	auto tryPropagate = [&](int nx, int ny, int nz, uint8_t lightToSet, bool includeYInSameChunkCheck) -> void
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
					return;

				neighborBlockIndex = getIndex(
					nx & CHUNK_LOWER_BITS_MASK,
					ny & CHUNK_LOWER_BITS_MASK,
					nz & CHUNK_LOWER_BITS_MASK
				);
			}

			auto& dstLight = neighborChunk->cells[neighborBlockIndex].lightLevel;
			if (dstLight.skyLight >= lightToSet)
				return;

			const BlockId neighborBlock = neighborChunk->cells[neighborBlockIndex].block;
			const auto* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
			if (!neighborBlockData || neighborBlockData->absorbsLight)
				return;

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
		};

	while (!LightPropagationStorage::threadLocalSkyLightPropagation.empty())
	{
		const auto data = LightPropagationStorage::threadLocalSkyLightPropagation.pop_and_return_unsafe();
		const int x = data.x;
		const int y = data.y;
		const int z = data.z;

		const size_t selfIndex = getIndex(x, y, z);
		const uint8_t skyLight = cells[selfIndex].lightLevel.skyLight;
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

				if (cells[belowIndex].lightLevel.skyLight == 15)
					break;

				const BlockId block = cells[belowIndex].block;
				const auto* blockData = AssetRegistry::getBlockData(block);
				if (!blockData || blockData->absorbsLight)
					break;

				cells[belowIndex].lightLevel.skyLight = 15;
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

					if (belowChunk->cells[belowIndex].lightLevel.skyLight < 15)
					{
						const auto* blockData = AssetRegistry::getBlockData(belowChunk->cells[belowIndex].block);
						if (blockData && !blockData->absorbsLight)
						{
							belowChunk->cells[belowIndex].lightLevel.skyLight = 15;
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
	TRACY_SCOPE_NC("Propagate block light removal", ProfileCategory::ChunkLight);

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
				neighborChunk->cells[neighborBlockIndex].lightLevel.blockLight = 0;
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
	TRACY_SCOPE_NC("Propagate sky light removal", ProfileCategory::ChunkLight);

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
				neighborChunk->cells[neighborBlockIndex].lightLevel.skyLight = 0;
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

	TRACY_SCOPE_NC("Build chunk light", ProfileCategory::ChunkLight);

	FenceGuard scopedFence(processingFence);

	// Reset light levels
	{
		TRACY_SCOPE_NC("Reset light levels", ProfileCategory::General);
		//std::memset(lightLevels, 0, sizeof(lightLevels));

		for (int i = 0; i < CHUNK_VOLUME; i++)
		{
			cells[i].lightLevel.fullByte = 0;
		}
	}

	const Chunk* topNeighbor = neighbors[getNeighborIndex(0, 1, 0)];

	// Collect block light sources
	{
		TRACY_SCOPE_NC("Collect block light sources", ProfileCategory::ChunkLight);
		for (int x = 0; x < CHUNK_SIZE; x++)
		for (int y = 0; y < CHUNK_SIZE; y++)
		for (int z = 0; z < CHUNK_SIZE; z++)
		{
			size_t index = getIndex(x, y, z);

			const auto* blockData = AssetRegistry::getBlockData(cells[index].block);
			if (!blockData)
			{
				continue;
			}

			uint8_t emission = blockData->lightEmission;
			if (emission == 0)
			{
				continue;
			}

			cells[index].lightLevel.blockLight = emission;
			LightPropagationStorage::threadLocalBlockLightPropagation.emplace(x, y, z);
		}
	}

	// Collect sky light sources
	if (!topNeighbor)
	{
		TRACY_SCOPE_NC("Collect sky light sources", ProfileCategory::ChunkLight);

		// Compute local heightmap for this chunk
		std::array<int, CHUNK_AREA> heightMap{};
		{
			TRACY_SCOPE_NC("Compute local heightmap", ProfileCategory::ChunkLight);
			heightMap.fill(-1);
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					for (int y = CHUNK_SIZE - 1; y >= 0; y--)
					{
						BlockId block = cells[getIndex(x, y, z)].block;
						const auto* blockData = AssetRegistry::getBlockData(block);
						if (!blockData || blockData->absorbsLight)
						{
							heightMap[getIndex(x, z)] = y;
							break;
						}
					}
				}
			}
		}
		
		// Compute array of heights from where to start adding nodes
		constexpr int MAX_LOCAL_HEIGHT = CHUNK_SIZE - 1;
		std::array<int, CHUNK_AREA> addNodeHeightMap;
		addNodeHeightMap.fill(MAX_LOCAL_HEIGHT); // Unfilled values will make nodes appear on every y coord
		// Coords won't be on border, so values on borders won't change
		{
			TRACY_SCOPE_NC("Compute height for nods", ProfileCategory::ChunkLight);
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
		}
		
		// Create nodes and fill light levels
		{
			TRACY_SCOPE_NC("Create nodes and fill light levels", ProfileCategory::ChunkLight);
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
						cells[getIndex(x, y, z)].lightLevel.skyLight = 15;
					}

					LightPropagationStorage::threadLocalSkyLightPropagation.emplace(x, localHeightToStartAddingNodes, z);
				}
			}
		}

		// // This code above can be simplified to this, but for some reason it results in slower execution
		// // Maybe because overhead of running propagateSkyLight
		//for (int x = 0; x < CHUNK_SIZE; x++)
		//{
		//	for (int z = 0; z < CHUNK_SIZE; z++)
		//	{
		//		size_t index = getIndex(x, CHUNK_SIZE - 1, z);
		//		BlockId block = cells[index].block;
		//		const auto* blockData = AssetRegistry::getBlockData(block);
		//		if (!blockData || blockData->absorbsLight)
		//		{
		//			continue;
		//		}
		//
		//		cells[index].lightLevel.skyLight = 15;
		//		LightPropagationStorage::threadLocalSkyLightPropagation.queue.emplace(x, CHUNK_SIZE - 1, z);
		//	}
		//}
	}

	// Collect light from neighbors
	// TODO: If neighbor block is solid, then check if it's a light source and propagate from it
	{
		TRACY_SCOPE_NC("Collect light levels from neighbors", ProfileCategory::ChunkLight);

		auto processNeighborFace = [&](int x, int y, int z, int nx, int ny, int nz, const Chunk* neighbor, bool propagatingFromTop)
			{
				size_t index = getIndex(x, y, z);
				const auto* blockData = AssetRegistry::getBlockData(cells[index].block);
				if (!blockData || blockData->absorbsLight)
				{
					return;
				}

				LightLevel& currentLight = cells[index].lightLevel;
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
	{
		TRACY_SCOPE_NC("Let neighbor chunks be updated", ProfileCategory::ChunkLight);
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

	TRACY_SCOPE_NC("Update chunk light", ProfileCategory::ChunkLight);

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
	{
		TRACY_SCOPE_NC("Let neighbor chunks be updated", ProfileCategory::ChunkLight);
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

	TRACY_SCOPE_NC("Update chunk mesh", ProfileCategory::ChunkMesh);

	// Collect visible faces
	{
		constexpr uint32_t transformationBitMasks[3] = { 0u, 0b11u, 0b111u };

		static const BlockData::TextureSlot fallbackTextureSlot(0, BlockData::TextureSlot::TextureTransformation::None, false);

		ChunkMeshFaceStorage::InstancesStorage localMeshInstances;
		//localMeshInstances.reserve(CHUNK_VOLUME / 4); TODO: For some reason make app freeze

		const glm::ivec3 globalChunkPosition = position << CHUNK_SIZE_LOG2;
		for (size_t currentBlockIndex = 0; currentBlockIndex < CHUNK_VOLUME; currentBlockIndex++)
		{
			glm::ivec3 currentBlockPosition = getPositionFromIndex(currentBlockIndex);

			// Generate new faces for this block
			BlockId block = cells[currentBlockIndex].block;
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
				const glm::ivec3 globalBlockPosition = globalChunkPosition + currentBlockPosition;
				const uint32_t hash = hash3(globalBlockPosition);
				for (const auto& face : model->alignedFaces)
				{
					// Get neighbor block coordinates
					const glm::ivec3 neighborBlockPosition = currentBlockPosition + DirectionsTable::directionsXYZ[face.normal];

					// Get neighbor chunk and block index
					size_t neighborBlockIndex;
					const Chunk* neighborChunk;

					const bool inSameChunk = ((neighborBlockPosition.x | neighborBlockPosition.y | neighborBlockPosition.z) & CHUNK_UPPER_BITS_MASK) == 0;

					if (inSameChunk)
					{
						neighborChunk = this;
						neighborBlockIndex = getIndex(neighborBlockPosition);
					}
					else
					{
						neighborChunk = neighbors[getSideNeighborIndex(face.normal)];
						if (!neighborChunk)
						{
							continue;
						}
						neighborBlockIndex = getIndex(neighborBlockPosition & CHUNK_LOWER_BITS_MASK);
					}

					// Get neighbor block and data
					BlockId neighborBlock = neighborChunk->cells[neighborBlockIndex].block;

					const BlockData* neighborBlockData = AssetRegistry::getBlockData(neighborBlock);
					if (!neighborBlockData || neighborBlockData->faceCulling[face.normal ^ 1])
					{
						continue;
					}

					if (block == neighborBlock && !blockData->faceCulling[face.normal])
					{
						continue;
					}

					// Calculate shading
					LightLevel neighborLight = neighborChunk->cells[neighborBlockIndex].lightLevel; // This line adds much to execution time, x5 in total

					ContextFaceAOAL aoData
					{
						.position = currentBlockPosition,
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
				BlockVertexData lightData;
				calculateBlockVertexLight(lightData, currentBlockPosition);

				UnalignedBlockFace instance;
				instance.blockX = currentBlockPosition.x;
				instance.blockY = currentBlockPosition.y;
				instance.blockZ = currentBlockPosition.z;

				// 24 light values: 6 faces x 4 vertices
				instance.light0 = lightData.light[0].fullByte;
				instance.light1 = lightData.light[1].fullByte;
				instance.light2 = lightData.light[2].fullByte;
				instance.light3 = lightData.light[3].fullByte;
				instance.light4 = lightData.light[4].fullByte;
				instance.light5 = lightData.light[5].fullByte;
				instance.light6 = lightData.light[6].fullByte;
				instance.light7 = lightData.light[7].fullByte;
				instance.light8 = lightData.light[8].fullByte;
				instance.light9 = lightData.light[9].fullByte;
				instance.light10 = lightData.light[10].fullByte;
				instance.light11 = lightData.light[11].fullByte;
				instance.light12 = lightData.light[12].fullByte;
				instance.light13 = lightData.light[13].fullByte;
				instance.light14 = lightData.light[14].fullByte;
				instance.light15 = lightData.light[15].fullByte;
				instance.light16 = lightData.light[16].fullByte;
				instance.light17 = lightData.light[17].fullByte;
				instance.light18 = lightData.light[18].fullByte;
				instance.light19 = lightData.light[19].fullByte;
				instance.light20 = lightData.light[20].fullByte;
				instance.light21 = lightData.light[21].fullByte;
				instance.light22 = lightData.light[22].fullByte;
				instance.light23 = lightData.light[23].fullByte;

				// 24 AO values: unpack each per-face packed byte (v0|v1<<2|v2<<4|v3<<6) into 4 x 2-bit fields
				instance.ao0 = (lightData.ao[0] >> 0) & 3; // face 0
				instance.ao1 = (lightData.ao[0] >> 2) & 3;
				instance.ao2 = (lightData.ao[0] >> 4) & 3;
				instance.ao3 = (lightData.ao[0] >> 6) & 3;
				instance.ao4 = (lightData.ao[1] >> 0) & 3; // face 1
				instance.ao5 = (lightData.ao[1] >> 2) & 3;
				instance.ao6 = (lightData.ao[1] >> 4) & 3;
				instance.ao7 = (lightData.ao[1] >> 6) & 3;
				instance.ao8 = (lightData.ao[2] >> 0) & 3; // face 2
				instance.ao9 = (lightData.ao[2] >> 2) & 3;
				instance.ao10 = (lightData.ao[2] >> 4) & 3;
				instance.ao11 = (lightData.ao[2] >> 6) & 3;
				instance.ao12 = (lightData.ao[3] >> 0) & 3; // face 3
				instance.ao13 = (lightData.ao[3] >> 2) & 3;
				instance.ao14 = (lightData.ao[3] >> 4) & 3;
				instance.ao15 = (lightData.ao[3] >> 6) & 3;
				instance.ao16 = (lightData.ao[4] >> 0) & 3; // face 4
				instance.ao17 = (lightData.ao[4] >> 2) & 3;
				instance.ao18 = (lightData.ao[4] >> 4) & 3;
				instance.ao19 = (lightData.ao[4] >> 6) & 3;
				instance.ao20 = (lightData.ao[5] >> 0) & 3; // face 5
				instance.ao21 = (lightData.ao[5] >> 2) & 3;
				instance.ao22 = (lightData.ao[5] >> 4) & 3;
				instance.ao23 = (lightData.ao[5] >> 6) & 3;

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

void Chunk::markAsShouldUpdateMesh() noexcept
{
	setFlag(Flag::ShouldUpdateMesh, true);
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

void Chunk::updateConnectivity()
{
	if (!chunkFlags.read(Flag::IsLoadedInWorld))
	{
		return;
	}

	TRACY_SCOPE_NC("Compute chunk connectivity", ProfileCategory::General);

	sideConnectivity.reset();

	{
		std::array<bool, CHUNK_VOLUME> visited{};

		struct StackEntry
		{
			glm::ivec3 pos;
			const BlockData* blockData;
		};
		std::vector<StackEntry> stack;
		stack.reserve(CHUNK_VOLUME);

		FenceGuard fence(processingFence);

		// Iterate boundary blocks
		glm::ivec3 startPos;
		for (startPos.x = 0; startPos.x < CHUNK_SIZE; startPos.x++)
		for (startPos.y = 0; startPos.y < CHUNK_SIZE; startPos.y++)
		for (startPos.z = 0; startPos.z < CHUNK_SIZE; startPos.z++)
		{
			bool isStartOnBoundary = false;
			isStartOnBoundary |= startPos.x == 0;
			isStartOnBoundary |= startPos.x == CHUNK_SIZE - 1;
			isStartOnBoundary |= startPos.y == 0;
			isStartOnBoundary |= startPos.y == CHUNK_SIZE - 1;
			isStartOnBoundary |= startPos.z == 0;
			isStartOnBoundary |= startPos.z == CHUNK_SIZE - 1;
			if (!isStartOnBoundary) continue;

			const int startIdx = getIndex(startPos);
			if (visited[startIdx]) continue;
			visited[startIdx] = true;

			// Solid / unknown blocks need no flood fill
			const BlockData* startData = AssetRegistry::getBlockData(cells[startIdx].block);
			if (!startData) continue;

			// Early-out: if the matrix is already fully connected, there is
			// nothing left to discover regardless of what this component touches.
			if (sideConnectivity.all()) break;

			std::array<bool, 6> touched{};

			stack.emplace_back(startPos, startData);

			while (!stack.empty())
			{
				auto cell = stack.back();
				stack.pop_back();
				const auto currentPosition = cell.pos;
				const BlockData* currentBlockData = cell.blockData;

				bool wasAbleToMoveOut = false;
				for (int dir = 0; dir < 6; dir++)
				{
					if (currentBlockData->faceCulling[dir]) continue;
					wasAbleToMoveOut = true;

					const glm::ivec3 neighborPos = currentPosition + DirectionsTable::directionsXYZ[dir];

					// Bounds check
					if (((neighborPos.x | neighborPos.y | neighborPos.z) & CHUNK_UPPER_BITS_MASK) != 0)
						continue;

					const int neighborIdx = getIndex(neighborPos);
					if (visited[neighborIdx]) continue;
					visited[neighborIdx] = true; // Mark as visited

					const BlockData* neighborData = AssetRegistry::getBlockData(cells[neighborIdx].block);
					if (!neighborData) continue;

					if (neighborData->faceCulling[dir ^ 1]) continue;

					stack.emplace_back(neighborPos, neighborData);
				}

				if (wasAbleToMoveOut)
				{
					// Record which chunk faces this cell touches
					touched[0] |= currentPosition.x == 0;
					touched[1] |= currentPosition.x == CHUNK_SIZE - 1;
					touched[2] |= currentPosition.y == 0;
					touched[3] |= currentPosition.y == CHUNK_SIZE - 1;
					touched[4] |= currentPosition.z == 0;
					touched[5] |= currentPosition.z == CHUNK_SIZE - 1;
				}
			}

			// Connect every pair of chunk faces this component can reach.
			for (int i = 0; i < 6; i++)
			{
				if (!touched[i]) continue;
				for (int j = i; j < 6; j++)
					if (touched[j]) sideConnectivity.set(i, j, true);
			}
		}
	}

	//{
	//	static std::mutex mtx;
	//	std::lock_guard lock(mtx);
	//
	//	for (int i = 0; i < 6; i++)
	//	{
	//		bool t = sideConnectivity.read(i, 2);
	//
	//		if (t)
	//		{
	//			std::cout << i << ", ";
	//		}
	//	}
	//	std::cout << "\n";
	//}

	// Print
	//{
	//	static std::mutex mtx;
	//	std::lock_guard lock(mtx);
	//
	//	std::cout << "Chunk:\n";
	//	//for (int i = 0; i < 6; i++)
	//	{
	//		int i = 3;
	//		for (int j = 0; j < 6; j++)
	//		{
	//			std::cout << sideConnectivity.read(i, j) << " ";
	//		}
	//		std::cout << "\n";
	//	}
	//}
}

void Chunk::markAsShouldUpdateConnectivity() noexcept
{
	setFlag(Flag::ShouldUpdateConnectivity, true);
	parentRegion->setFlag(ChunkRegion::Flag::HasConnectivityToUpdate, true);
	ChunkRegion::setGlobalFlag(ChunkRegion::Flag::HasConnectivityToUpdate, true);
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

void Chunk::applyNeighborDirtyMask(uint32_t mask) noexcept
{
	TRACY_SCOPE_NC("Apply neighbor dirty mask", ProfileCategory::General);

	// Using a loop with bit manipulation because it should reduce the number of branching and memory fetchesd

	constexpr size_t neighborCount = sizeof(neighbors) / sizeof(neighbors[0]); // Idk, neighbor.size() doesn't work (because 'this' can't be used as constant)
	constexpr uint32_t clearTrashMask = (1u << neighborCount) - 1; // Mask to clear bits that are out of bounds
	mask &= clearTrashMask;

	while (mask)
	{
		int i = std::countr_zero(mask); // Get index of lowest set bit
		if (neighbors[i]) neighbors[i]->markAsShouldUpdateMesh();
		mask &= mask - 1; // Clear lowest set bit
	}
}

std::pair<BlockId, LightLevel> Chunk::getBlockAndLightAt(int x, int y, int z) const noexcept
{
	size_t index = getIndex(x, y, z);
	return std::make_pair(cells[index].block, cells[index].lightLevel);
}

std::pair<BlockId, LightLevel> Chunk::getBlockAndLightAt(const glm::ivec3& pos) const noexcept
{
	size_t index = getIndex(pos);
	return std::make_pair(cells[index].block, cells[index].lightLevel);
}

void Chunk::setBlockAt(int x, int y, int z, BlockId block, bool saveBlockChanges)
{
	size_t index = getIndex(x, y, z);

	BlockId previousBlock = cells[index].block;
	if (previousBlock == block)
	{
		return;
	}

	// Update array
	cells[index].block = block;

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

	// TODO: Now both blocks can absorb light, but can have different faceCulling values
	if (previousBlockData->absorbsLight && !newBlockData->absorbsLight)
	{
		const glm::ivec3 currentBlockPosition = glm::ivec3(x, y, z);

		// Collect maximum light level from neighbors and propagate it to this block
		uint8_t maxBlockLightToSet = 0;
		uint8_t maxSkyLightToSet = 0;
		for (int i = 0; i < 6; i++)
		{
			glm::ivec3 npos = currentBlockPosition + DirectionsTable::directionsXYZ[i];

			size_t neighborIndex;
			Chunk* neighborChunk = traverseToSideNeighbor(npos.x, npos.y, npos.z, i, neighborIndex);
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
			cells[index].lightLevel.blockLight = maxBlockLightToSet;
			if (maxBlockLightToSet > 1) addBlockLightPropagationNode(x, y, z);
		}
		if (maxSkyLightToSet > 0)
		{
			cells[index].lightLevel.skyLight = maxSkyLightToSet;
			if (maxSkyLightToSet > 1) addSkyLightPropagationNode(x, y, z);
		}
	}
	else if (!previousBlockData->absorbsLight && newBlockData->absorbsLight)
	{
		// Remove light at this block
		uint8_t currentBlockLight = cells[index].lightLevel.blockLight;
		if (currentBlockLight > 0)
		{
			cells[index].lightLevel.blockLight = 0;
			addBlockLightRemovalNode(x, y, z, currentBlockLight);
		}
		uint8_t currentSkyLight = cells[index].lightLevel.skyLight;
		if (currentSkyLight > 0)
		{
			cells[index].lightLevel.skyLight = 0;
			addSkyLightRemovalNode(x, y, z, currentSkyLight);
		}
	}

	// Handle light emission changes
	if (previousEmission != newEmission)
	{
		if (previousEmission > newEmission)
		{
			cells[index].lightLevel.blockLight = 0;
			addBlockLightRemovalNode(x, y, z, previousEmission);
		}

		if (newEmission > 0)
		{
			cells[index].lightLevel.blockLight = newEmission;
			addBlockLightPropagationNode(x, y, z);
		}
	}

	// Mark meshes as dirty
	markMeshesDirtyAroundBlock(x, y, z);

	// Mark connectivity as dirty
	if constexpr (USE_CONNECTIVITY_TESTING)
	{
		markAsShouldUpdateConnectivity();
	}
	
	// Allow update light
	if (lightPropagation.hasNodes())
	{
		parentRegion->setFlag(ChunkRegion::Flag::HasLightToUpdate, true);
		ChunkRegion::setGlobalFlag(ChunkRegion::Flag::HasLightToUpdate, true);
	}
}

void Chunk::setLightAt(int x, int y, int z, LightLevel lightValue)
{
	cells[getIndex(x, y, z)].lightLevel = lightValue;
	markMeshesDirtyAroundBlock(x, y, z);
}

void Chunk::setBlockLightAt(int x, int y, int z, uint8_t lightLevel)
{
	cells[getIndex(x, y, z)].lightLevel.blockLight = lightLevel;
	markMeshesDirtyAroundBlock(x, y, z);
}

void Chunk::setSkyLightAt(int x, int y, int z, uint8_t lightLevel)
{
	cells[getIndex(x, y, z)].lightLevel.skyLight = lightLevel;
	markMeshesDirtyAroundBlock(x, y, z);
}

void Chunk::setLightAt(size_t index, LightLevel lightValue)
{
	cells[index].lightLevel = lightValue;

	glm::ivec3 pos = getPositionFromIndex(index);
	markMeshesDirtyAroundBlock(pos.x, pos.y, pos.z);
}

void Chunk::setBlockLightAt(size_t index, uint8_t lightLevel)
{
	cells[index].lightLevel.blockLight = lightLevel;

	glm::ivec3 pos = getPositionFromIndex(index);
	markMeshesDirtyAroundBlock(pos.x, pos.y, pos.z);
}

void Chunk::setSkyLightAt(size_t index, uint8_t lightLevel)
{
	cells[index].lightLevel.skyLight = lightLevel;

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
	LightLevel localLightLevels[4];

	auto x = context.position.x;
	auto y = context.position.y;
	auto z = context.position.z;
	auto normal = context.normal;
	auto centerFaceLight = context.centerFaceLight;

	auto getSafe = [this, &neighborData](size_t dataIdx, int x_, int y_, int z_, int fcn)
		{
			size_t index;
			const Chunk* chunk = traverseThroughNeighbors(x_, y_, z_, index);

			if (!chunk) return;

			BlockId block = chunk->cells[index].block;
			LightLevel lightLevel = chunk->cells[index].lightLevel;

			neighborData[dataIdx].lightLevel = lightLevel;

			const auto* blockData = AssetRegistry::getBlockData(block);
			neighborData[dataIdx].isSolid = blockData && blockData->faceCulling[fcn];
		};

	auto getSafe2 = [this, &neighborData](size_t dataIdx, int x_, int y_, int z_, int fcn1, int fcn2)
		{
			size_t index;
			const Chunk* chunk = traverseThroughNeighbors(x_, y_, z_, index);

			if (!chunk) return;

			BlockId block = chunk->cells[index].block;
			LightLevel lightLevel = chunk->cells[index].lightLevel;

			neighborData[dataIdx].lightLevel = lightLevel;

			const auto* blockData = AssetRegistry::getBlockData(block);
			neighborData[dataIdx].isSolid = blockData && (blockData->faceCulling[fcn1] || blockData->faceCulling[fcn2]);
		};

	switch (normal)
	{
	case 0: // -X face
		x--;
		getSafe2(0, x, y - 1, z - 1, 3, 5);
		getSafe (1, x, y - 1, z    , 3);
		getSafe2(2, x, y - 1, z + 1, 3, 4);
		getSafe (3, x, y    , z - 1, 5);
		getSafe (4, x, y    , z + 1, 4);
		getSafe2(5, x, y + 1, z - 1, 2, 5);
		getSafe (6, x, y + 1, z	   , 2);
		getSafe2(7, x, y + 1, z + 1, 2, 4);

		calculateVertexAmbientOcclusionAndLight(ao0, localLightLevels[0], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, localLightLevels[1], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, localLightLevels[2], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, localLightLevels[3], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		break;
	case 1: // +X face
		x++;
		getSafe2(0, x, y - 1, z - 1, 3, 5);
		getSafe (1, x, y - 1, z    , 3);
		getSafe2(2, x, y - 1, z + 1, 3, 4);
		getSafe (3, x, y    , z - 1, 5);
		getSafe (4, x, y    , z + 1, 4);
		getSafe2(5, x, y + 1, z - 1, 2, 5);
		getSafe (6, x, y + 1, z    , 2);
		getSafe2(7, x, y + 1, z + 1, 2, 4);

		calculateVertexAmbientOcclusionAndLight(ao0, localLightLevels[0], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, localLightLevels[1], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, localLightLevels[2], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, localLightLevels[3], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 2: // -Y face
		y--;
		getSafe2(0, x - 1, y, z - 1, 1, 5);
		getSafe (1, x    , y, z - 1, 5);
		getSafe2(2, x + 1, y, z - 1, 0, 5);
		getSafe (3, x - 1, y, z    , 1);
		getSafe (4, x + 1, y, z    , 0);
		getSafe2(5, x - 1, y, z + 1, 1, 4);
		getSafe (6, x    , y, z + 1, 4);
		getSafe2(7, x + 1, y, z + 1, 0, 4);

		calculateVertexAmbientOcclusionAndLight(ao0, localLightLevels[0], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, localLightLevels[1], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, localLightLevels[2], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, localLightLevels[3], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 3: // +Y face
		y++;
		getSafe2(0, x - 1, y, z - 1, 1, 5);
		getSafe (1, x    , y, z - 1, 5);
		getSafe2(2, x + 1, y, z - 1, 0, 5);
		getSafe (3, x - 1, y, z    , 1);
		getSafe (4, x + 1, y, z    , 0);
		getSafe2(5, x - 1, y, z + 1, 1, 4);
		getSafe (6, x    , y, z + 1, 4);
		getSafe2(7, x + 1, y, z + 1, 0, 4);

		calculateVertexAmbientOcclusionAndLight(ao0, localLightLevels[0], centerFaceLight, neighborData[4], neighborData[1], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, localLightLevels[1], centerFaceLight, neighborData[3], neighborData[1], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, localLightLevels[2], centerFaceLight, neighborData[3], neighborData[6], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, localLightLevels[3], centerFaceLight, neighborData[4], neighborData[6], neighborData[7]);
		break;
	case 4: // -Z face
		z--;
		getSafe2(0, x - 1, y - 1, z, 1, 3);
		getSafe (1, x    , y - 1, z, 3);
		getSafe2(2, x + 1, y - 1, z, 0, 3);
		getSafe (3, x - 1, y    , z, 1);
		getSafe (4, x + 1, y    , z, 0);
		getSafe2(5, x - 1, y + 1, z, 1, 2);
		getSafe (6, x    , y + 1, z, 2);
		getSafe2(7, x + 1, y + 1, z, 0, 2);

		calculateVertexAmbientOcclusionAndLight(ao0, localLightLevels[0], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, localLightLevels[1], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, localLightLevels[2], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, localLightLevels[3], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 5: // +Z face
		z++;
		getSafe2(0, x - 1, y - 1, z, 1, 3);
		getSafe (1, x    , y - 1, z, 3);
		getSafe2(2, x + 1, y - 1, z, 0, 3);
		getSafe (3, x - 1, y    , z, 1);
		getSafe (4, x + 1, y    , z, 0);
		getSafe2(5, x - 1, y + 1, z, 1, 2);
		getSafe (6, x    , y + 1, z, 2);
		getSafe2(7, x + 1, y + 1, z, 0, 2);

		calculateVertexAmbientOcclusionAndLight(ao0, localLightLevels[0], centerFaceLight, neighborData[1], neighborData[3], neighborData[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, localLightLevels[1], centerFaceLight, neighborData[1], neighborData[4], neighborData[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, localLightLevels[2], centerFaceLight, neighborData[6], neighborData[4], neighborData[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, localLightLevels[3], centerFaceLight, neighborData[6], neighborData[3], neighborData[5]);
		break;
	}

	context.outAmbientOcclusion = ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6);

	auto& light = context.outLightLevel;

	static_assert(sizeof(light) >= sizeof(localLightLevels), "light packing too small");
	std::memcpy(&light, localLightLevels, sizeof(light));
}

void Chunk::calculateVertexAmbientOcclusionAndLightUnaligned(unsigned int& ao, LightLevel& light, const LightLevelAndIsSolid& center, const LightLevelAndIsSolid& side1, const LightLevelAndIsSolid& side2, const LightLevelAndIsSolid& corner) const
{
	constexpr std::array<unsigned int, 5> magicNumbers = {
		0u,
		getMagicNumberForDivision(1, 8),
		getMagicNumberForDivision(2, 8),
		getMagicNumberForDivision(3, 8),
		getMagicNumberForDivision(4, 8)
	};

	const unsigned bothSolid = side1.isSolid & side2.isSolid;
	const unsigned use1 = !side1.isSolid;
	const unsigned use2 = !side2.isSolid;
	const unsigned useC = !corner.isSolid && !bothSolid;
	const unsigned useCenter = !center.isSolid;

	const unsigned blockLightSum =
		useCenter * center.lightLevel.blockLight +
		use1 * side1.lightLevel.blockLight +
		use2 * side2.lightLevel.blockLight +
		useC * corner.lightLevel.blockLight;

	const unsigned skyLightSum =
		useCenter * center.lightLevel.skyLight +
		use1 * side1.lightLevel.skyLight +
		use2 * side2.lightLevel.skyLight +
		useC * corner.lightLevel.skyLight;

	const unsigned count = useCenter + use1 + use2 + useC;
	const unsigned magicNumber = magicNumbers[count];
	light.blockLight = (blockLightSum * magicNumber) >> 8u;
	light.skyLight = (skyLightSum * magicNumber) >> 8u;

	const unsigned sideAo = (1u - bothSolid) * (use1 + use2 + useC);
	ao = std::max((int)sideAo - center.isSolid, 0);
}

void Chunk::calculateFaceAmbientOcclusionAndLightUnaligned(ContextFaceAOAL& context) const
{
	LightLevelAndIsSolid neighborData[8];

	unsigned int ao0 = 0, ao1 = 0, ao2 = 0, ao3 = 0;
	LightLevel localLightLevels[4];

	auto x = context.position.x;
	auto y = context.position.y;
	auto z = context.position.z;
	auto normal = context.normal;
	LightLevelAndIsSolid centerFaceData = { context.centerFaceLight, context.centerFaceIsSolid };

	// getSafe / getSafe2 lambdas — identical to the aligned version
	auto getSafe = [this, &neighborData](size_t dataIdx, int x_, int y_, int z_, int fcn)
		{
			size_t index;
			const Chunk* chunk = traverseThroughNeighbors(x_, y_, z_, index);
			if (!chunk) return;
			BlockId    block = chunk->cells[index].block;
			LightLevel lightLevel = chunk->cells[index].lightLevel;
			neighborData[dataIdx].lightLevel = lightLevel;
			const auto* blockData = AssetRegistry::getBlockData(block);
			neighborData[dataIdx].isSolid = blockData && blockData->faceCulling[fcn];
		};

	auto getSafe2 = [this, &neighborData](size_t dataIdx, int x_, int y_, int z_, int fcn1, int fcn2)
		{
			size_t index;
			const Chunk* chunk = traverseThroughNeighbors(x_, y_, z_, index);
			if (!chunk) return;
			BlockId    block = chunk->cells[index].block;
			LightLevel lightLevel = chunk->cells[index].lightLevel;
			neighborData[dataIdx].lightLevel = lightLevel;
			const auto* blockData = AssetRegistry::getBlockData(block);
			neighborData[dataIdx].isSolid = blockData && (blockData->faceCulling[fcn1] || blockData->faceCulling[fcn2]);
		};

	// Helper alias — reduces noise in the switch below
	auto calcVertex = [&](unsigned int& ao, LightLevel& ll,
		const LightLevelAndIsSolid& s1,
		const LightLevelAndIsSolid& s2,
		const LightLevelAndIsSolid& c)
		{
			calculateVertexAmbientOcclusionAndLightUnaligned(
				ao, ll, centerFaceData, s1, s2, c);
		};

	switch (normal)
	{
	case 0: // -X face
		x--;
		getSafe2(0, x, y - 1, z - 1, 3, 5);  getSafe(1, x, y - 1, z, 3);
		getSafe2(2, x, y - 1, z + 1, 3, 4);  getSafe(3, x, y, z - 1, 5);
		getSafe(4, x, y, z + 1, 4);     getSafe2(5, x, y + 1, z - 1, 2, 5);
		getSafe(6, x, y + 1, z, 2);     getSafe2(7, x, y + 1, z + 1, 2, 4);

		calcVertex(ao0, localLightLevels[0], neighborData[1], neighborData[3], neighborData[0]);
		calcVertex(ao1, localLightLevels[1], neighborData[1], neighborData[4], neighborData[2]);
		calcVertex(ao2, localLightLevels[2], neighborData[6], neighborData[4], neighborData[7]);
		calcVertex(ao3, localLightLevels[3], neighborData[6], neighborData[3], neighborData[5]);
		break;
	case 1: // +X face
		x++;
		getSafe2(0, x, y - 1, z - 1, 3, 5);  getSafe(1, x, y - 1, z, 3);
		getSafe2(2, x, y - 1, z + 1, 3, 4);  getSafe(3, x, y, z - 1, 5);
		getSafe(4, x, y, z + 1, 4);     getSafe2(5, x, y + 1, z - 1, 2, 5);
		getSafe(6, x, y + 1, z, 2);     getSafe2(7, x, y + 1, z + 1, 2, 4);

		calcVertex(ao0, localLightLevels[0], neighborData[1], neighborData[4], neighborData[2]);
		calcVertex(ao1, localLightLevels[1], neighborData[1], neighborData[3], neighborData[0]);
		calcVertex(ao2, localLightLevels[2], neighborData[6], neighborData[3], neighborData[5]);
		calcVertex(ao3, localLightLevels[3], neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 2: // -Y face
		y--;
		getSafe2(0, x - 1, y, z - 1, 1, 5);  getSafe(1, x, y, z - 1, 5);
		getSafe2(2, x + 1, y, z - 1, 0, 5);  getSafe(3, x - 1, y, z, 1);
		getSafe(4, x + 1, y, z, 0);     getSafe2(5, x - 1, y, z + 1, 1, 4);
		getSafe(6, x, y, z + 1, 4);     getSafe2(7, x + 1, y, z + 1, 0, 4);

		calcVertex(ao0, localLightLevels[0], neighborData[1], neighborData[4], neighborData[2]);
		calcVertex(ao1, localLightLevels[1], neighborData[1], neighborData[3], neighborData[0]);
		calcVertex(ao2, localLightLevels[2], neighborData[6], neighborData[3], neighborData[5]);
		calcVertex(ao3, localLightLevels[3], neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 3: // +Y face
		y++;
		getSafe2(0, x - 1, y, z - 1, 1, 5);  getSafe(1, x, y, z - 1, 5);
		getSafe2(2, x + 1, y, z - 1, 0, 5);  getSafe(3, x - 1, y, z, 1);
		getSafe(4, x + 1, y, z, 0);     getSafe2(5, x - 1, y, z + 1, 1, 4);
		getSafe(6, x, y, z + 1, 4);     getSafe2(7, x + 1, y, z + 1, 0, 4);

		calcVertex(ao0, localLightLevels[0], neighborData[4], neighborData[1], neighborData[2]);
		calcVertex(ao1, localLightLevels[1], neighborData[3], neighborData[1], neighborData[0]);
		calcVertex(ao2, localLightLevels[2], neighborData[3], neighborData[6], neighborData[5]);
		calcVertex(ao3, localLightLevels[3], neighborData[4], neighborData[6], neighborData[7]);
		break;
	case 4: // -Z face
		z--;
		getSafe2(0, x - 1, y - 1, z, 1, 3);  getSafe(1, x, y - 1, z, 3);
		getSafe2(2, x + 1, y - 1, z, 0, 3);  getSafe(3, x - 1, y, z, 1);
		getSafe(4, x + 1, y, z, 0);     getSafe2(5, x - 1, y + 1, z, 1, 2);
		getSafe(6, x, y + 1, z, 2);     getSafe2(7, x + 1, y + 1, z, 0, 2);

		calcVertex(ao0, localLightLevels[0], neighborData[1], neighborData[4], neighborData[2]);
		calcVertex(ao1, localLightLevels[1], neighborData[1], neighborData[3], neighborData[0]);
		calcVertex(ao2, localLightLevels[2], neighborData[6], neighborData[3], neighborData[5]);
		calcVertex(ao3, localLightLevels[3], neighborData[6], neighborData[4], neighborData[7]);
		break;
	case 5: // +Z face
		z++;
		getSafe2(0, x - 1, y - 1, z, 1, 3);  getSafe(1, x, y - 1, z, 3);
		getSafe2(2, x + 1, y - 1, z, 0, 3);  getSafe(3, x - 1, y, z, 1);
		getSafe(4, x + 1, y, z, 0);     getSafe2(5, x - 1, y + 1, z, 1, 2);
		getSafe(6, x, y + 1, z, 2);     getSafe2(7, x + 1, y + 1, z, 0, 2);

		calcVertex(ao0, localLightLevels[0], neighborData[1], neighborData[3], neighborData[0]);
		calcVertex(ao1, localLightLevels[1], neighborData[1], neighborData[4], neighborData[2]);
		calcVertex(ao2, localLightLevels[2], neighborData[6], neighborData[4], neighborData[7]);
		calcVertex(ao3, localLightLevels[3], neighborData[6], neighborData[3], neighborData[5]);
		break;
	}

	context.outAmbientOcclusion = ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6);

	static_assert(sizeof(context.outLightLevel) >= sizeof(localLightLevels));
	std::memcpy(&context.outLightLevel, localLightLevels, sizeof(context.outLightLevel));
}

void Chunk::calculateBlockVertexLight(BlockVertexData& result, const glm::ivec3& currentBlockPosition) const
{
	for (int face = 0; face < 6; face++)
	{
		const glm::ivec3 neighborPos = currentBlockPosition + DirectionsTable::directionsXYZ[face];

		BlockId centerFaceBlock = {};
		LightLevel centerFaceLight = {};

		if (((neighborPos.x | neighborPos.y | neighborPos.z) & CHUNK_UPPER_BITS_MASK) == 0)
		{
			auto index = getIndex(neighborPos);
			centerFaceBlock = cells[index].block;
			centerFaceLight = cells[index].lightLevel;
		}
		else
		{
			const Chunk* neighborChunk = neighbors[getSideNeighborIndex(face)];
			if (neighborChunk)
			{
				auto index = getIndex(neighborPos & CHUNK_LOWER_BITS_MASK);
				centerFaceBlock = neighborChunk->cells[index].block;
				centerFaceLight = neighborChunk->cells[index].lightLevel;
			}
		}

		const BlockData* centerFaceBlockData = AssetRegistry::getBlockData(centerFaceBlock);
		const bool centerFaceIsSolid = centerFaceBlockData && centerFaceBlockData->faceCulling[face ^ 1];

		ContextFaceAOAL aoData
		{
			.position = currentBlockPosition,
			.normal = face,
			.centerFaceLight = centerFaceLight,
			.centerFaceIsSolid = centerFaceIsSolid
		};

		calculateFaceAmbientOcclusionAndLightUnaligned(aoData);

		result.ao[face] = static_cast<uint8_t>(aoData.outAmbientOcclusion);

		static_assert(sizeof(aoData.outLightLevel) == 4 * sizeof(LightLevel));
		std::memcpy(&result.light[face * 4], &aoData.outLightLevel, sizeof(aoData.outLightLevel));
	}
}
