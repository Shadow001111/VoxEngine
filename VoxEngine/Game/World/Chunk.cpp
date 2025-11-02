#include "Chunk.h"
#include "Chunk/TerrainGenerator.h"
#include "Chunk/ChunkMeshManager.h"

#include "Core/Profiler.h"
#include "Core/SymmetricBitMatrix.h"

#include <cassert>
#include <vector>
#include <iostream>

#define CHUNK_SMOOTH_LIGHTING 1

BlockTextureIDDatabase Chunk::blockTextureDatabase;
std::vector<MeshData*> Chunk::pendingMeshUploads;

inline size_t Chunk::getIndex(int x, int y, int z)
{
	return (x << 8) | (y << 4) | z;
}

glm::ivec3 Chunk::getPositionFromIndex(size_t index)
{
	return {
		(index >> 8) & 15,
		(index >> 4) & 15,
		index & 15
	};
}

Chunk::Chunk()
{
}

Chunk::~Chunk()
{
	destroy(); // Just in case
}

inline bool Chunk::operator==(const Chunk& other) const
{
	return position == other.position;
}

// Prepares chunk for use
void Chunk::init(int x, int y, int z, Chunk** neighbors)
{
	// Set position
	position = { x, y, z };

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
	setState(Chunk::State::NeedsBlocks);
	isLoadedInWorld.store(true, std::memory_order_release);
	isLoadedChunkColumnData.store(false, std::memory_order_release);
	areBlocksBuilt.store(false, std::memory_order_release);
	isLightBuilt.store(false, std::memory_order_release);

	meshData.resetFaceCount();
	meshData.opaqueDirty = false;
	meshData.transparentDirty = false;

	cameraClosestBlockPosForSortingMesh = -1;
	shouldSortMeshAfterBuild = false;
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
	isLoadedInWorld.store(false, std::memory_order_release);
	meshData.opaqueInstances.clear();
	meshData.transparentInstances.clear();

	// Release chunk column data
	if (isLoadedChunkColumnData.load(std::memory_order_acquire))
	{
		isLoadedChunkColumnData.store(false, std::memory_order_release);
		TerrainGenerator::getInstance().unloadChunkColumnData(position.x, position.z);
	}

	//
	{
		std::lock_guard<std::mutex> lock(lightNodeMutex);
		while (!lightNodeContainer.empty())
		{
			lightNodeContainer.pop();
		}
	}
	{
		std::lock_guard<std::mutex> lock(lightRemovalNodeMutex);
		while (!lightRemovalNodeContainer.empty())
		{
			lightRemovalNodeContainer.pop();
		}
	}
}

// Fills 'blocks' array
void Chunk::buildBlocks()
{
	if (
		areBlocksBuilt.load(std::memory_order_acquire) ||
		!isLoadedInWorld.load(std::memory_order_acquire)
	)
	{
		return;
	}

	Profiler::beginProfile("Build chunk blocks: wait", ProfileCategory::ChunkBlocks);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	// Load chunk column data
	const ChunkColumnData* chunkColumnData = TerrainGenerator::getInstance().loadChunkColumnData(position.x, position.z);
	const int* heightMap = chunkColumnData->heightMapRead();
	isLoadedChunkColumnData.store(true, std::memory_order_release);

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
				int localheight = std::min(CHUNK_SIZE - 1, globalHeight - globalChunkY);
				for (int y = 0; y < CHUNK_SIZE; y++)
				{
					int globalY = globalChunkY + y;

					size_t index = getIndex(x, y, z);

					Block block = Block::Air;
					if (globalY > globalHeight)
					{
						block = Block::Air;
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

		if ((position.x + position.z) & 1)
		{
			blocks[getIndex(7, 7, 7)] = Block::ColoredGlass;
		}
	}

	// Caves
	if (computeCaveMask)
	{
		bool caveMask[CHUNK_VOLUME];
		TerrainGenerator::getInstance().computeCaveMask(caveMask, position.x, position.y, position.z);

		PROFILE_SCOPE("Build chunk blocks: caves", ProfileCategory::ChunkBlocks);

		for (int i = 0; i < CHUNK_VOLUME; i++)
		{
			if (blocks[i] == Block::Stone && caveMask[i])
			{
				blocks[i] = Block::Air;
			}
		}
	}

	areBlocksBuilt.store(true, std::memory_order_release);

	//computeConnectivity();
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

		bool regionConnectivity[6]; // TODO: Can be a bitset
		for (int i = 0; i < 6; i++)
		{
			regionConnectivity[i] = false;
		}

		cellsToVisit.push_back(startPos);
		while (!cellsToVisit.empty())
		{
			// Get cell
			auto cell = cellsToVisit.back();
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
				if (!BlockDataBase::getBlockData(block)->properties.areFacesTransparent)
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

void Chunk::buildLight()
{
	if (
		!isLoadedInWorld.load(std::memory_order_acquire) ||
		!areBlocksBuilt.load(std::memory_order_acquire)
		)
	{
		return;
	}

	Profiler::beginProfile("Build chunk light: wait", ProfileCategory::ChunkLight);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	//
	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };

	// Initialize all light values to 0
	std::fill(std::begin(lightLevels), std::end(lightLevels), LightLevel(0, 0));

	// Step 1: Collect light sources
	std::queue<LightNode> localLightNodeContainer;
	{
		PROFILE_SCOPE("Build chunk light: collect light sources", ProfileCategory::ChunkLight);

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

					lightLevels[index].blockLight = emission;
					localLightNodeContainer.emplace(x, y, z);
				}
			}
		}
	}

	// Step 2: Collect light from neighbors
	{
		PROFILE_SCOPE("Build chunk light: collect neighbor light", ProfileCategory::ChunkLight);
		
		const Chunk* neighbor;;

		// -X
		neighbor = neighbors[0];
		if (neighbor)
		{
			const int x = 0;
			const int neighborX = CHUNK_SIZE - 1;
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					size_t index = getIndex(x, y, z);
					if (GET_BLOCK_PROPERTIES(blocks[index]).absorbsLight)
					{
						continue;
					}
					LightLevel neighborLight = neighbor->getLightAt(neighborX, y, z);
					if (lightLevels[index].blockLight + 2 > neighborLight.blockLight)
					{
						continue;
					}
					lightLevels[index].blockLight = neighborLight.blockLight - 1;
					localLightNodeContainer.emplace(x, y, z);
				}
			}
		}

		// +X
		neighbor = neighbors[1];
		if (neighbor)
		{
			const int x = CHUNK_SIZE - 1;
			const int neighborX = 0;
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					size_t index = getIndex(x, y, z);
					if (GET_BLOCK_PROPERTIES(blocks[index]).absorbsLight)
					{
						continue;
					}
					LightLevel neighborLight = neighbor->getLightAt(neighborX, y, z);
					if (lightLevels[index].blockLight + 2 > neighborLight.blockLight)
					{
						continue;
					}
					lightLevels[index].blockLight = neighborLight.blockLight - 1;
					localLightNodeContainer.emplace(x, y, z);
				}
			}
		}

		// -Y
		neighbor = neighbors[2];
		if (neighbor)
		{
			const int y = 0;
			const int neighborY = CHUNK_SIZE - 1;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					size_t index = getIndex(x, y, z);
					if (GET_BLOCK_PROPERTIES(blocks[index]).absorbsLight)
					{
						continue;
					}
					LightLevel neighborLight = neighbor->getLightAt(x, neighborY, z);
					if (lightLevels[index].blockLight + 2 > neighborLight.blockLight)
					{
						continue;
					}
					lightLevels[index].blockLight = neighborLight.blockLight - 1;
					localLightNodeContainer.emplace(x, y, z);
				}
			}
		}

		// +Y
		neighbor = neighbors[3];
		if (neighbor)
		{
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
					LightLevel neighborLight = neighbor->getLightAt(x, neighborY, z);
					if (lightLevels[index].blockLight + 2 > neighborLight.blockLight)
					{
						continue;
					}
					lightLevels[index].blockLight = neighborLight.blockLight - 1;
					localLightNodeContainer.emplace(x, y, z);
				}
			}
		}

		// -Z
		neighbor = neighbors[4];
		if (neighbor)
		{
			const int z = 0;
			const int neighborZ = CHUNK_SIZE - 1;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int y = 0; y < CHUNK_SIZE; y++)
				{
					size_t index = getIndex(x, y, z);
					if (GET_BLOCK_PROPERTIES(blocks[index]).absorbsLight)
					{
						continue;
					}
					LightLevel neighborLight = neighbor->getLightAt(x, y, neighborZ);
					if (lightLevels[index].blockLight + 2 > neighborLight.blockLight)
					{
						continue;
					}
					lightLevels[index].blockLight = neighborLight.blockLight - 1;
					localLightNodeContainer.emplace(x, y, z);
				}
			}
		}

		// +Z
		neighbor = neighbors[5];
		if (neighbor)
		{
			const int z = CHUNK_SIZE - 1;
			const int neighborZ = 0;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int y = 0; y < CHUNK_SIZE; y++)
				{
					size_t index = getIndex(x, y, z);
					if (GET_BLOCK_PROPERTIES(blocks[index]).absorbsLight)
					{
						continue;
					}
					LightLevel neighborLight = neighbor->getLightAt(x, y, neighborZ);
					if (lightLevels[index].blockLight + 2 > neighborLight.blockLight)
					{
						continue;
					}
					lightLevels[index].blockLight = neighborLight.blockLight - 1;
					localLightNodeContainer.emplace(x, y, z);
				}
			}
		}
	}

	// Step 3: Propagate light using flood-fill
	{
		PROFILE_SCOPE("Build chunk light: light propagation", ProfileCategory::ChunkLight);

		// TODO: Consider using vector to speed up. Must remove front element!
		while (!localLightNodeContainer.empty())
		{
			// Get node data
			const auto& data = localLightNodeContainer.front();
			int x = data.x;
			int y = data.y;
			int z = data.z;
			size_t index = getIndex(x, y, z);
			localLightNodeContainer.pop();

			// Get light level
			LightLevel lightLevel = lightLevels[index];
			if (lightLevel.blockLight < 2)
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
				const LightLevel neighborLight = neighborChunk->getLightAt(neighborIndex);

				if (GET_BLOCK_PROPERTIES(neighborBlock).absorbsLight)
				{
					continue;
				}

				if (neighborLight.blockLight > lightLevel.blockLight - 2)
				{
					continue;
				}

				if (neighborChunk == this)
				{
					lightLevels[getIndex(nx, ny, nz)].blockLight = lightLevel.blockLight - 1;
					localLightNodeContainer.emplace(nx, ny, nz);
				}
				else
				{
					int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

					// TODO: This and the same line in UpdateLight should cause chunk to update its mesh
					neighborChunk->setBlockLightAt(neighborIndex, lightLevel.blockLight - 1);
					neighborChunk->addLightNodeToQueue(neighborNX, neighborNY, neighborNZ);
				}
			}
		}
	}

	isLightBuilt.store(true, std::memory_order_release);
}

void Chunk::buildMesh()
{
	if (
		!isLoadedInWorld.load(std::memory_order_acquire) ||
		!areBlocksBuilt.load(std::memory_order_acquire) ||
		!isLightBuilt.load(std::memory_order_acquire)
		)
	{
		return;
	}

	Profiler::beginProfile("Build chunk mesh: wait", ProfileCategory::ChunkMesh);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	{
		PROFILE_SCOPE("Build chunk mesh", ProfileCategory::ChunkMesh);

		// Collect visible faces
		std::vector<BlockFaceInstance> opaqueInstances;
		std::vector<BlockFaceInstance> transparentInstances;
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					Block block = getBlockAt(x, y, z);
					const BlockData* blockData = GET_BLOCK_DATA(block);
					if (!blockData->properties.hasFaces)
					{
						continue;
					}

					const auto& textureIDs = blockTextureDatabase.getBlockTextureIDs(block);
					auto& instances = blockData->properties.areFacesTransparent ? transparentInstances : opaqueInstances;
					const BlockData* neighborBlockData;

					size_t neighborIndex;
					const Chunk* neighborChunk;
					Block neighborBlock;
					LightLevel neighborLight;

					// -X
					// TODO: If neighbor is missing, don't generate faces
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x - 1, y, z, 0, neighborIndex);
					neighborBlock = neighborChunk ? neighborChunk->getBlockAt(neighborIndex) : Block::Air;
					neighborLight = neighborChunk ? neighborChunk->getLightAt(neighborIndex) : LightLevel(0, 15);

					if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 0, neighborLight);
						instances.emplace_back(
							x, y, z,
							0,
							ao,
							textureIDs.ids[0],
							blockData->textures.texturesTransformation & 3,
							light
						);
					}

					// +X
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x + 1, y, z, 1, neighborIndex);
					neighborBlock = neighborChunk ? neighborChunk->getBlockAt(neighborIndex) : Block::Air;
					neighborLight = neighborChunk ? neighborChunk->getLightAt(neighborIndex) : LightLevel(0, 15);

					if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 1, neighborLight);
						instances.emplace_back(
							x, y, z,
							1,
							ao,
							textureIDs.ids[1],
							(blockData->textures.texturesTransformation >> 2) & 3,
							light);
					}

					// -Y
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x, y - 1, z, 2, neighborIndex);
					neighborBlock = neighborChunk ? neighborChunk->getBlockAt(neighborIndex) : Block::Air;
					neighborLight = neighborChunk ? neighborChunk->getLightAt(neighborIndex) : LightLevel(0, 15);

					if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 2, neighborLight);
						instances.emplace_back(
							x, y, z,
							2,
							ao,
							textureIDs.ids[2],
							(blockData->textures.texturesTransformation >> 4) & 3,
							light
						);
					}

					// +Y
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x, y + 1, z, 3, neighborIndex);
					neighborBlock = neighborChunk ? neighborChunk->getBlockAt(neighborIndex) : Block::Air;
					neighborLight = neighborChunk ? neighborChunk->getLightAt(neighborIndex) : LightLevel(0, 15);

					if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 3, neighborLight);
						instances.emplace_back(
							x, y, z,
							3,
							ao,
							textureIDs.ids[3],
							(blockData->textures.texturesTransformation >> 6) & 3,
							light
						);
					}

					// -Z
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x, y, z - 1, 4, neighborIndex);
					neighborBlock = neighborChunk ? neighborChunk->getBlockAt(neighborIndex) : Block::Air;
					neighborLight = neighborChunk ? neighborChunk->getLightAt(neighborIndex) : LightLevel(0, 15);

					if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 4, neighborLight);
						instances.emplace_back(
							x, y, z,
							4,
							ao,
							textureIDs.ids[4],
							(blockData->textures.texturesTransformation >> 8) & 3,
							light
						);
					}

					// +Z
					neighborChunk = getChunkAndIndex_checkSideNeighbor(x, y, z + 1, 5, neighborIndex);
					neighborBlock = neighborChunk ? neighborChunk->getBlockAt(neighborIndex) : Block::Air;
					neighborLight = neighborChunk ? neighborChunk->getLightAt(neighborIndex) : LightLevel(0, 15);

					if (block != neighborBlock && (neighborBlockData = GET_BLOCK_DATA(neighborBlock))->properties.areFacesTransparent)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 5, neighborLight);
						instances.emplace_back(
							x, y, z,
							5,
							ao,
							textureIDs.ids[5],
							(blockData->textures.texturesTransformation >> 10) & 3,
							light);
					}
				}
			}
		}

		// Check if chunk got unloaded by the time we were building mesh
		if (!isLoadedInWorld.load(std::memory_order_acquire))
		{
			return;
		}

		// Set mesh data
		ScopedProcessingFence scopedFence(meshData.processingFence);

		meshData.opaqueDirty = false;
		meshData.transparentDirty = false;

		meshData.opaqueInstances = std::move(opaqueInstances);
		meshData.opaqueFaceCount = meshData.opaqueInstances.size();

		meshData.transparentInstances = std::move(transparentInstances);
		meshData.transparentFaceCount = meshData.transparentInstances.size();

		if (meshData.opaqueFaceCount > 0)
		{
			meshData.opaqueDirty = true;
		}
		if (meshData.transparentFaceCount > 0)
		{
			meshData.transparentDirty = true;
			if (meshData.transparentFaceCount > 1)
			{
				shouldSortMeshAfterBuild = true;
				cameraClosestBlockPosForSortingMesh = -1;
			}
		}

		if (!(meshData.opaqueDirty || meshData.transparentDirty))
		{
			meshData.updateRenderFaceCount();
		}
	}
}

std::bitset<27> Chunk::updateLight()
{
	if (
		!isLoadedInWorld.load(std::memory_order_acquire) ||
		!areBlocksBuilt.load(std::memory_order_acquire)
		)
	{
		return false;
	}

	Profiler::beginProfile("Update chunk light: wait", ProfileCategory::ChunkLight);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	PROFILE_SCOPE("Update chunk light", ProfileCategory::ChunkLight);

	//
	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };

	std::bitset<27> lightChanged;

	constexpr auto index3x3x3 = [](int dx, int dy, int dz) noexcept
		{
		return (dx + 1) * 9 + (dy + 1) * 3 + (dz + 1);
		};

	//TODO: Instead of immediate neighbor calls, collect and batch

	std::queue<LightNode> localLightNodeContainer;
	{
		std::lock_guard<std::mutex> lock(lightNodeMutex);
		localLightNodeContainer.swap(lightNodeContainer);
	}
	std::queue<LightRemovalNode> localLightRemovalNodeContainer;
	{
		std::lock_guard<std::mutex> lock(lightRemovalNodeMutex);
		localLightRemovalNodeContainer.swap(lightRemovalNodeContainer);
	}

	{
		PROFILE_SCOPE("Update chunk light: light removal", ProfileCategory::ChunkLight);

		while (!localLightRemovalNodeContainer.empty())
		{
			// Get node data
			const auto& data = localLightRemovalNodeContainer.front();
			int x = data.x;
			int y = data.y;
			int z = data.z;
			uint8_t nodeLightLevel = data.lightLevel;
			size_t index = getIndex(x, y, z);
			localLightRemovalNodeContainer.pop();

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
				const LightLevel neighborLight = neighborChunk->getLightAt(neighborIndex);

				if (GET_BLOCK_PROPERTIES(neighborBlock).absorbsLight)
				{
					continue;
				}

				if (neighborLight.blockLight > 0 && neighborLight.blockLight < nodeLightLevel)
				{
					if (neighborChunk == this)
					{
						lightLevels[getIndex(nx, ny, nz)].blockLight = 0;
						localLightRemovalNodeContainer.emplace(nx, ny, nz, neighborLight.blockLight);

						const bool onBorder[6] = {
							nx == 0,
							nx == (CHUNK_SIZE - 1),
							ny == 0,
							ny == (CHUNK_SIZE - 1),
							nz == 0,
							nz == (CHUNK_SIZE - 1)
						};

						std::bitset<27> localLightChanged;
						constexpr size_t centerIndex = index3x3x3(0, 0, 0);
						localLightChanged.set(centerIndex, true);
						size_t bitIndex = 0;
						for (int dx = -1; dx <= 1; dx++)
						{
							for (int dy = -1; dy <= 1; dy++)
							{
								for (int dz = -1; dz <= 1; dz++)
								{
									if (bitIndex == centerIndex)
									{
										bitIndex++;
										continue;
									}

									bool changed = true;

									if (dx < 0) changed &= onBorder[0];
									else if (dx > 0) changed &= onBorder[1];
									if (dy < 0) changed &= onBorder[2];
									else if (dy > 0) changed &= onBorder[3];
									if (dz < 0) changed &= onBorder[4];
									else if (dz > 0) changed &= onBorder[5];

									localLightChanged.set(bitIndex, changed);
									bitIndex++;
								}
							}
						}
						lightChanged |= localLightChanged;
					}
					else
					{
						int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
						int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
						int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

						neighborChunk->setBlockLightAt(neighborNX, neighborNY, neighborNZ, 0);
						neighborChunk->addLightRemovalNodeToQueue(neighborNX, neighborNY, neighborNZ, neighborLight.blockLight);
					}
				}
				else if (neighborLight.blockLight >= nodeLightLevel)
				{
					if (neighborChunk == this)
					{
						localLightNodeContainer.emplace(nx, ny, nz);
					}
					else
					{
						int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
						int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
						int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

						neighborChunk->addLightNodeToQueue(neighborNX, neighborNY, neighborNZ);
					}
				}
			}
		}
	}

	{
		PROFILE_SCOPE("Update chunk light: light propagation", ProfileCategory::ChunkLight);

		// TODO: Consider using vector to speed up. Must remove front element!
		while (!localLightNodeContainer.empty())
		{
			// Get node data
			const auto& data = localLightNodeContainer.front();
			int x = data.x;
			int y = data.y;
			int z = data.z;
			size_t index = getIndex(x, y, z);
			localLightNodeContainer.pop();

			// Get light level
			LightLevel lightLevel = lightLevels[index];
			if (lightLevel.blockLight < 2)
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
				const LightLevel neighborLight = neighborChunk->getLightAt(neighborIndex);

				if (GET_BLOCK_PROPERTIES(neighborBlock).absorbsLight)
				{
					continue;
				}

				if (neighborLight.blockLight + 2 > lightLevel.blockLight)
				{
					continue;
				}

				int checkX = nx & CHUNK_UPPER_BITS_MASK;
				int checkY = ny & CHUNK_UPPER_BITS_MASK;
				int checkZ = nz & CHUNK_UPPER_BITS_MASK;

				if (neighborChunk == this)
				{
					lightLevels[getIndex(nx, ny, nz)].blockLight = lightLevel.blockLight - 1;
					localLightNodeContainer.emplace(nx, ny, nz);

					const bool onBorder[6] = {
							nx == 0,
							nx == (CHUNK_SIZE - 1),
							ny == 0,
							ny == (CHUNK_SIZE - 1),
							nz == 0,
							nz == (CHUNK_SIZE - 1)
					};

					std::bitset<27> localLightChanged;
					constexpr size_t centerIndex = index3x3x3(0, 0, 0);
					localLightChanged.set(centerIndex, true);
					size_t bitIndex = 0;
					for (int dx = -1; dx <= 1; dx++)
					{
						for (int dy = -1; dy <= 1; dy++)
						{
							for (int dz = -1; dz <= 1; dz++)
							{
								if (bitIndex == centerIndex)
								{
									bitIndex++;
									continue;
								}

								bool changed = true;

								if (dx < 0) changed &= onBorder[0];
								else if (dx > 0) changed &= onBorder[1];
								if (dy < 0) changed &= onBorder[2];
								else if (dy > 0) changed &= onBorder[3];
								if (dz < 0) changed &= onBorder[4];
								else if (dz > 0) changed &= onBorder[5];

								localLightChanged.set(bitIndex, changed);
								bitIndex++;
							}
						}
					}
					lightChanged |= localLightChanged;
				}
				else
				{
					int neighborNX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborNY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborNZ = nz & CHUNK_LOWER_BITS_MASK;

					neighborChunk->setBlockLightAt(neighborNX, neighborNY, neighborNZ, lightLevel.blockLight - 1);
					neighborChunk->addLightNodeToQueue(neighborNX, neighborNY, neighborNZ);
				}
			}
		}
	}
	return lightChanged;
}

bool Chunk::hasLightUpdates() const
{
	{
		std::lock_guard<std::mutex> lock(lightNodeMutex);
		if (!lightNodeContainer.empty())
		{
			return true;
		}
	}
	{
		std::lock_guard<std::mutex> lock(lightRemovalNodeMutex);
		if (!lightRemovalNodeContainer.empty())
		{
			return true;
		}
	}
	return false;
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

	// TODO: Let chunk update light when sorting mesh.
	Profiler::beginProfile("Sort chunk mesh: wait", ProfileCategory::ChunkMesh);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	PROFILE_SCOPE("Sort chunk mesh", ProfileCategory::ChunkMesh);

	// Sorting only transparent faces for now
	// If GL_CULL_FACE is disabled, sorting must be adjusted. If faces have same position, they should be sorted by prioritized normal(something opposite to camera direction).
	// TODO: Consider sorting opaque faces to reduce overdraw

	const size_t instanceCount = meshData.transparentInstances.size();
	std::vector<std::pair<uint8_t, uint16_t>> distanceIndexPairs;
	distanceIndexPairs.reserve(instanceCount);

	// Collect
	for (size_t i = 0; i < instanceCount; ++i)
	{
		const auto& instance = meshData.transparentInstances[i];

		glm::ivec3 pos;
		instance.decodePosition(pos.x, pos.y, pos.z);

		glm::ivec3 delta = glm::abs(pos - calculateDistanceFrom);
		unsigned int manhattanDistance = delta.x + delta.y + delta.z;

		distanceIndexPairs.emplace_back(manhattanDistance, i);
	}

	// Sorting faces in descending order, so furthest transparent faces will be rendered first, for blending
	std::sort(distanceIndexPairs.begin(), distanceIndexPairs.end(),
		[](const auto& a, const auto& b)
		{
			return a.first > b.first;
		});

	// Reordering instances
	std::vector<BlockFaceInstance> reordered;
	reordered.reserve(instanceCount);

	for (const auto& pair : distanceIndexPairs)
	{
		reordered.push_back(std::move(meshData.transparentInstances[pair.second]));
	}

	meshData.transparentInstances = std::move(reordered);
	meshData.transparentDirty = true;
}

bool Chunk::shouldMeshBeSorted(bool cameraMoved) const
{
	return meshData.transparentFaceCount > 1 && (cameraMoved || shouldSortMeshAfterBuild);
}

void Chunk::askForMeshUpload()
{
	if (!(meshData.opaqueDirty || meshData.transparentDirty))
	{
		return;
	}

	pendingMeshUploads.push_back(&meshData);
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
	if (!meshData.created)
	{
		return false;
	}

	const size_t faceCount = meshData.getRenderFaceCount();
	return
		faceCount > 0 &&
		faceCount <= meshData.getFaceCapacity();
}

const Chunk* Chunk::getChunkAndIndex_checkSideNeighbor(int x, int y, int z, int side, size_t& outIndex) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx == 0 && ny == 0 && nz == 0)
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
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	return blocks[getIndex(x, y, z)];
}

LightLevel Chunk::getLightAt(int x, int y, int z) const
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	return lightLevels[getIndex(x, y, z)];
}

std::pair<Block, LightLevel> Chunk::getBlockAndLightAt(int x, int y, int z) const
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	size_t index = getIndex(x, y, z);
	return std::make_pair(blocks[index], lightLevels[index]);
}

Block Chunk::getBlockAt(size_t index) const
{
	assert(index < CHUNK_VOLUME);
	return blocks[index];
}

LightLevel Chunk::getLightAt(size_t index) const
{
	assert(index < CHUNK_VOLUME);
	return lightLevels[index];
}

std::pair<Block, LightLevel> Chunk::getBlockAndLightAt(size_t index) const
{
	assert(index < CHUNK_VOLUME);
	return std::make_pair(blocks[index], lightLevels[index]);
}

void Chunk::setBlockAt(int x, int y, int z, Block block)
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	blocks[getIndex(x, y, z)] = block;
}

void Chunk::setBlockAt_updateLight(int x, int y, int z, Block block)
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);

	size_t index = getIndex(x, y, z);

	Block previousBlock = blocks[index];
	if (previousBlock == block)
	{
		return;
	}

	blocks[index] = block;

	const BlockData* previousBlockData = BlockDataBase::getBlockData(previousBlock);
	uint8_t previousEmission = previousBlockData->properties.lightEmission;

	const BlockData* newBlockData = BlockDataBase::getBlockData(block);
	uint8_t newEmission = newBlockData->properties.lightEmission;

	if (previousEmission == newEmission)
	{
		return;
	}

	lightLevels[index].blockLight = newEmission;

	if (previousEmission > newEmission)
	{
		addLightRemovalNodeToQueue(x, y, z, previousEmission);
		lightLevels[index].blockLight = 0;
	}

	if (newEmission > 0)
	{
		addLightNodeToQueue(x, y, z);
	}

	// TODO: Implement proper light propagation when blocking block is removing
}

void Chunk::setLightAt(int x, int y, int z, LightLevel lightValue)
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	lightLevels[getIndex(x, y, z)] = lightValue;
}

void Chunk::setBlockLightAt(int x, int y, int z, uint8_t lightLevel)
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	lightLevels[getIndex(x, y, z)].blockLight = lightLevel;
}

void Chunk::setSkyLightAt(int x, int y, int z, uint8_t lightLevel)
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	lightLevels[getIndex(x, y, z)].skyLight = lightLevel;
}

void Chunk::setBlockAt(size_t index, Block block)
{
	assert(index < CHUNK_VOLUME);
	blocks[index] = block;
}

void Chunk::setLightAt(size_t index, LightLevel lightValue)
{
	assert(index < CHUNK_VOLUME);
	lightLevels[index] = lightValue;
}

void Chunk::setBlockLightAt(size_t index, uint8_t lightLevel)
{
	assert(index < CHUNK_VOLUME);
	lightLevels[index].blockLight = lightLevel;
}

void Chunk::setSkyLightAt(size_t index, uint8_t lightLevel)
{
	assert(index < CHUNK_VOLUME);
	lightLevels[index].skyLight = lightLevel;
}

void Chunk::addLightNodeToQueue(int x, int y, int z)
{
	std::lock_guard<std::mutex> lock(lightNodeMutex);
	lightNodeContainer.emplace(x, y, z);
}

void Chunk::addLightRemovalNodeToQueue(int x, int y, int z, uint8_t lightLevel)
{
	std::lock_guard<std::mutex> lock(lightRemovalNodeMutex);
	lightRemovalNodeContainer.emplace(x, y, z, lightLevel);
}

void Chunk::calculateVertexAmbientOcclusionAndLight(unsigned int& ao, LightLevel& light, LightLevel centerLight, LightLevel side1Light, LightLevel side2Light, LightLevel cornerLight, bool side1Solid, bool side2Solid, bool cornerSolid) const
{
	// TODO: Maybe fix smoothlighting. It's strange when light values are in checkerboard.
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

	assert(avgBlockLight >= 0 && avgBlockLight <= 15);
	assert(avgSkyLight >= 0 && avgSkyLight <= 15);

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

	auto getSafe = [this, &data](size_t dataIdx, int x_, int y_, int z_)
		{
			size_t idx;
			const Chunk* c = getChunkAndIndex_checkNeighborsTraverse(x_, y_, z_, idx);
			data[dataIdx] = c ? c->getBlockAndLightAt(idx) : std::make_pair(Block::Air, LightLevel(0, 15));
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

		for (int i = 0; i < 8; i++)
		{
			n[i] = !GET_BLOCK_PROPERTIES(data[i].first).areFacesTransparent;
		}

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

		for (int i = 0; i < 8; i++)
		{
			n[i] = !GET_BLOCK_PROPERTIES(data[i].first).areFacesTransparent;
		}

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

		for (int i = 0; i < 8; i++)
		{
			n[i] = !GET_BLOCK_PROPERTIES(data[i].first).areFacesTransparent;
		}

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

		for (int i = 0; i < 8; i++)
		{
			n[i] = !GET_BLOCK_PROPERTIES(data[i].first).areFacesTransparent;
		}

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

		for (int i = 0; i < 8; i++)
		{
			n[i] = !GET_BLOCK_PROPERTIES(data[i].first).areFacesTransparent;
		}

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

		for (int i = 0; i < 8; i++)
		{
			n[i] = !GET_BLOCK_PROPERTIES(data[i].first).areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, lightLevels[0], centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, lightLevels[1], centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, lightLevels[2], centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, lightLevels[3], centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		break;
	}

	//
	ao = ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6);
	light = *((unsigned int*)lightLevels); // TODO: Light values can be in reversed order!
}

int Chunk::getX() const
{
	return position.x;
}

int Chunk::getY() const
{
	return position.y;
}

int Chunk::getZ() const
{
	return position.z;
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
	return isLoadedInWorld.load(std::memory_order_acquire);
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

		size_t opaqueFaceCount = chunkMesh->opaqueFaceCount;
		size_t transparentFaceCount = chunkMesh->transparentFaceCount;

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

LightLevel::LightLevel(LightLevel&& other) :
	blockLight(other.blockLight), skyLight(other.skyLight)
{
}

LightLevel& LightLevel::operator=(const LightLevel& other)
{
	blockLight = other.blockLight;
	skyLight = other.skyLight;
	return *this;
}

LightLevel& LightLevel::operator=(LightLevel&& other) noexcept
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
//Int3Hasher

size_t Int3Hasher::operator()(const glm::ivec3& other) const
{
	constexpr size_t addConst = 0x9e3779b97f4a7c15;
	size_t h = (size_t)other.x + addConst;
	h ^= (size_t)other.y + addConst + (h << 6) + (h >> 2);
	h ^= (size_t)other.z + addConst + (h << 6) + (h >> 2);
	return h;
}

//============================================================================
//DrawArraysIndirectCommand

DrawArraysIndirectCommand::DrawArraysIndirectCommand(unsigned int count, unsigned int instanceCount, unsigned int first, unsigned int baseInstance) :
	count(count), instanceCount(instanceCount), first(first), baseInstance(baseInstance)
{
}

//============================================================================
