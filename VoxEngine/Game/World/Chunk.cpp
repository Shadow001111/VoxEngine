#include "Chunk.h"
#include "Chunk/TerrainGenerator.h"
#include "Chunk/ChunkMeshManager.h"

#include "Core/Profiler.h"

#include <cassert>
#include <vector>
#include <iostream>

BlockTextureIDDatabase Chunk::blockTextureDatabase;
std::vector<MeshData*> Chunk::pendingMeshUploads;

inline size_t Chunk::getIndex(int x, int y, int z)
{
	return (x << 8) | (y << 4) | z;
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
		std::lock_guard<std::mutex> lock(lightMutex);
		while (!lightNodeContainer.empty())
		{
			lightNodeContainer.pop_back();
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

		constexpr int border = 4;
		for (int x = border; x < CHUNK_SIZE - border; x++)
		{
			for (int y = border; y < CHUNK_SIZE - border; y++)
			{
				for (int z = border; z < CHUNK_SIZE - border; z++)
				{
					if ((x + y + z) & 1)
					{
						blocks[getIndex(x, y, z)] = Block::ColoredGlass;
					}
				}
			}
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
	std::fill(std::begin(light), std::end(light), 0);

	// Step 1: Collect light sources
	std::vector<LightNode> localLightNodeContainer;
	{
		PROFILE_SCOPE("Build chunk light: collect light sources", ProfileCategory::ChunkLight);

		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					size_t index = getIndex(x, y, z);
					light[index] = 15 << 4;

					Block currentBlock = blocks[index];
					const BlockData* currentBlockData = BlockDataBase::getBlockData(currentBlock);

					uint8_t emission = currentBlockData->properties.lightEmission;
					if (emission == 0)
					{
						continue;
					}

					if (currentBlockData->properties.areFacesTransparent)
					{
						localLightNodeContainer.emplace_back(x, y, z, emission, -1);
					}

					for (int i = 0; i < 6; i++)
					{
						int nx = x + dx[i];
						int ny = y + dy[i];
						int nz = z + dz[i];

						if ((nx & CHUNK_UPPER_BITS_MASK) == 0 &&
							(ny & CHUNK_UPPER_BITS_MASK) == 0 &&
							(nz & CHUNK_UPPER_BITS_MASK) == 0)
						{
							localLightNodeContainer.emplace_back(nx, ny, nz, emission, i);
						}
						else
						{
							Chunk* neighbor = neighbors[i];
							if (neighbor == nullptr)
							{
								continue;
							}

							int neighborLocalX = nx & CHUNK_LOWER_BITS_MASK;
							int neighborLocalY = ny & CHUNK_LOWER_BITS_MASK;
							int neighborLocalZ = nz & CHUNK_LOWER_BITS_MASK;

							neighbor->addNodeToLightQueue(neighborLocalX, neighborLocalY, neighborLocalZ, emission, i);
						}
					}
				}
			}
		}
	}

	// Step 2: Propagate light using flood-fill
	{
		PROFILE_SCOPE("Build chunk light: flood-fill", ProfileCategory::ChunkLight);

		while (!localLightNodeContainer.empty())
		{
			auto data = localLightNodeContainer.front();
			localLightNodeContainer.pop_back();

			// Get node data
			int x = data.x;
			int y = data.y;
			int z = data.z;
			uint8_t blockLight = data.lightLevel & 15;
			int8_t propagationSide = data.propagationSide;

			// Get block data
			size_t index = getIndex(x, y, z);
			Block block = blocks[index];
			const BlockData* blockData = BlockDataBase::getBlockData(block);

			uint8_t lightAbsorption = blockData->properties.lightAbsorption;
			assert(lightAbsorption > 0);

			// Calculate new light
			uint8_t newBlockLight;
			if (propagationSide == -1)
			{
				newBlockLight = blockLight;
			}
			else
			{
				newBlockLight = blockLight > lightAbsorption ? blockLight - lightAbsorption : 0;
			}

			if (newBlockLight == 0)
			{
				continue;
			}

			// Get current light
			uint8_t currentLight = light[index];
			uint8_t currentBlockLight = currentLight & 15;
			uint8_t currentSkyLight = currentLight << 4;

			// Compare light values
			if (newBlockLight <= currentBlockLight)
			{
				continue;
			}

			// Store new value
			light[index] = currentSkyLight | newBlockLight;

			// Early exit, because spreading light value of 1 will do nothing
			if (newBlockLight == 1)
			{
				continue;
			}

			// Propagate to 6 neighbors
			for (int i = 0; i < 6; i++)
			{
				if (i == (propagationSide ^ 1))
				{
					continue;
				}

				int nx = x + dx[i];
				int ny = y + dy[i];
				int nz = z + dz[i];

				if ((nx & CHUNK_UPPER_BITS_MASK) == 0 &&
					(ny & CHUNK_UPPER_BITS_MASK) == 0 &&
					(nz & CHUNK_UPPER_BITS_MASK) == 0)
				{
					localLightNodeContainer.emplace_back(nx, ny, nz, newBlockLight, i);
				}
				else
				{
					Chunk* neighbor = neighbors[i];
					if (neighbor == nullptr)
					{
						continue;
					}

					int neighborLocalX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborLocalY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborLocalZ = nz & CHUNK_LOWER_BITS_MASK;

					neighbor->addNodeToLightQueue(neighborLocalX, neighborLocalY, neighborLocalZ, newBlockLight, i);
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
					Block block = getBlock_inBoundaries(x, y, z);
					const BlockData* blockData = BlockDataBase::getBlockData(block);
					if (!blockData->properties.hasFaces)
					{
						continue;
					}

					const auto& textureIDs = blockTextureDatabase.getBlockTextureIDs(block);
					auto& instances = blockData->properties.areFacesTransparent ? transparentInstances : opaqueInstances;

					// -X
					std::pair<Block, uint8_t> blockAndLight = getBlockAndLight_checkSideNeighbor(x - 1, y, z, 0);
					const BlockData* neighborBlockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (neighborBlockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 0, blockAndLight.second);
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
					blockAndLight = getBlockAndLight_checkSideNeighbor(x + 1, y, z, 1);
					neighborBlockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (neighborBlockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 1, blockAndLight.second);
						instances.emplace_back(
							x, y, z,
							1,
							ao,
							textureIDs.ids[1],
							(blockData->textures.texturesTransformation >> 2) & 3,
							light);
					}

					// -Y
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y - 1, z, 2);
					neighborBlockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (neighborBlockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 2, blockAndLight.second);
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
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y + 1, z, 3);
					neighborBlockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (neighborBlockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 3, blockAndLight.second);
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
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y, z - 1, 4);
					neighborBlockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (neighborBlockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 4, blockAndLight.second);
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
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y, z + 1, 5);
					neighborBlockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (neighborBlockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 5, blockAndLight.second);
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
			shouldSortMeshAfterBuild = meshData.transparentFaceCount > 1;
		}
	}
}

void Chunk::updateLight()
{
	if (
		!isLoadedInWorld.load(std::memory_order_acquire) ||
		!areBlocksBuilt.load(std::memory_order_acquire)
		)
	{
		return;
	}

	Profiler::beginProfile("Update chunk light: wait", ProfileCategory::ChunkLight);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	PROFILE_SCOPE("Update chunk light", ProfileCategory::ChunkLight);

	// Get pending light updates
	std::vector<LightNode> localLightQueue;
	{
		std::lock_guard<std::mutex> lock(lightMutex);
		if (lightNodeContainer.empty())
		{
			return;
		}
		localLightQueue.swap(lightNodeContainer);
	}

	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };

	bool lightChanged = false;

	// Process light propagation
	while (!localLightQueue.empty())
	{
		auto data = localLightQueue.front();
		localLightQueue.pop_back();

		// Get node data
		int x = data.x;
		int y = data.y;
		int z = data.z;
		uint8_t blockLight = data.lightLevel & 15;
		int8_t propagationSide = data.propagationSide;

		// Get block data
		size_t index = getIndex(x, y, z);
		Block block = blocks[index];
		const BlockData* blockData = BlockDataBase::getBlockData(block);

		uint8_t lightAbsorption = blockData->properties.lightAbsorption;
		assert(lightAbsorption > 0);

		// Calculate new light
		uint8_t newBlockLight;
		if (propagationSide == -1)
		{
			newBlockLight = blockLight;
		}
		else
		{
			newBlockLight = blockLight > lightAbsorption ? blockLight - lightAbsorption : 0;
		}

		if (newBlockLight == 0)
		{
			continue;
		}

		// Get current light
		uint8_t currentLight = light[index];
		uint8_t currentBlockLight = currentLight & 15;
		uint8_t currentSkyLight = currentLight << 4;

		// Compare light values
		if (newBlockLight <= currentBlockLight)
		{
			continue;
		}

		// Store new value
		light[index] = currentSkyLight | newBlockLight;

		// Early exit, because spreading light value of 1 will do nothing
		if (newBlockLight == 1)
		{
			continue;
		}

		// Propagate to 6 neighbors
		for (int i = 0; i < 6; i++)
		{
			if (i == (propagationSide ^ 1))
			{
				continue;
			}

			int nx = x + dx[i];
			int ny = y + dy[i];
			int nz = z + dz[i];

			if ((nx & CHUNK_UPPER_BITS_MASK) == 0 &&
				(ny & CHUNK_UPPER_BITS_MASK) == 0 &&
				(nz & CHUNK_UPPER_BITS_MASK) == 0)
			{
				localLightQueue.emplace_back(nx, ny, nz, newBlockLight, i);
			}
			else
			{
				Chunk* neighbor = neighbors[i];
				if (neighbor == nullptr)
				{
					continue;
				}

				int neighborLocalX = nx & CHUNK_LOWER_BITS_MASK;
				int neighborLocalY = ny & CHUNK_LOWER_BITS_MASK;
				int neighborLocalZ = nz & CHUNK_LOWER_BITS_MASK;

				neighbor->addNodeToLightQueue(neighborLocalX, neighborLocalY, neighborLocalZ, newBlockLight, i);
			}
		}
	}

	// If light changed, mark for mesh rebuild
	if (lightChanged && getState() == State::Ready)
	{
		setState(State::NeedsMesh);
	}
}

bool Chunk::hasLightUpdates() const
{
	std::lock_guard<std::mutex> lock(lightMutex);
	return !lightNodeContainer.empty();
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
	size_t faceCount = meshData.opaqueFaceCount;
	if (faceCount == 0)
	{
		return;
	}
	drawCommands.emplace_back(4, faceCount, 0, meshData.allocatedBlock.offset);
	positions.push_back(position);
}

void Chunk::collectTransparentRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const
{
	size_t faceCount = meshData.transparentFaceCount;
	if (meshData.transparentFaceCount == 0)
	{
		return;
	}
	drawCommands.emplace_back(4, faceCount, 0, meshData.allocatedBlock.offset + meshData.opaqueFaceCount);
	positions.push_back(position);
}

bool Chunk::canBeRendered() const
{
	if (!meshData.created)
	{
		return false;
	}

	size_t faceCount = meshData.getFaceCount();
	return
		faceCount > 0 &&
		faceCount <= meshData.getFaceCapacity();
}

// Function doesn't check for boundaries, it trusts the caller. On debug mode, it asserts.
Block Chunk::getBlock_inBoundaries(int x, int y, int z) const
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	return blocks[getIndex(x, y, z)];
}

Block Chunk::getBlock_checkSideNeighbor(int x, int y, int z, int side) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx == 0 && ny == 0 && nz == 0)
	{
		return blocks[getIndex(x, y, z)];
	}

	const Chunk* neighbor = neighbors[side];
	if (neighbor)
	{
		return neighbor->getBlock_inBoundaries(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
	}
	return Block::Air;
}

// Function checks neighbors, if out of boundaries. Handles diagonal neighbors too.
Block Chunk::getBlock_checkNeighborsTraverse(int x, int y, int z) const
{
	// Note: Traverse may fail depending on traversal order. Isn't critical for ambient occlusion computing.

	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	// If within current chunk bounds
	if (nx == 0 && ny == 0 && nz == 0)
	{
		return blocks[getIndex(x, y, z)];
	}

	// Determine which direction(s) we need to traverse
	int dirX = (nx < 0) ? 0 : ((nx > 0) ? 1 : -1); // -1 means no X traversal
	int dirY = (ny < 0) ? 2 : ((ny > 0) ? 3 : -1); // -1 means no Y traversal
	int dirZ = (nz < 0) ? 4 : ((nz > 0) ? 5 : -1); // -1 means no Z traversal

	const Chunk* neighbor = this;

	// Traverse in X direction first
	if (dirX != -1)
	{
		neighbor = neighbor->neighbors[dirX];
		if (!neighbor) return Block::Air;
	}

	// Then traverse in Y direction
	if (dirY != -1)
	{
		neighbor = neighbor->neighbors[dirY];
		if (!neighbor) return Block::Air;
	}

	// Finally traverse in Z direction
	if (dirZ != -1)
	{
		neighbor = neighbor->neighbors[dirZ];
		if (!neighbor) return Block::Air;
	}

	// Get local coordinates in the final neighbor chunk
	int localX = x & CHUNK_LOWER_BITS_MASK;
	int localY = y & CHUNK_LOWER_BITS_MASK;
	int localZ = z & CHUNK_LOWER_BITS_MASK;

	return neighbor->getBlock_inBoundaries(localX, localY, localZ);
}

void Chunk::setBlock_inBoundaries(int x, int y, int z, Block block)
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	blocks[getIndex(x, y, z)] = block;
}

uint8_t Chunk::getLight_inBoundaries(int x, int y, int z) const
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	return light[getIndex(x, y, z)];
}

uint8_t Chunk::getLight_checkSideNeighbor(int x, int y, int z, int side) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx == 0 && ny == 0 && nz == 0)
	{
		return light[getIndex(x, y, z)];
	}

	const Chunk* neighbor = neighbors[side];
	if (neighbor)
	{
		return neighbor->getLight_inBoundaries(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
	}
	return 15 << 4;
}

uint8_t Chunk::getLight_checkNeighborsTraverse(int x, int y, int z) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	// If within current chunk bounds
	if (nx == 0 && ny == 0 && nz == 0)
	{
		return light[getIndex(x, y, z)];
	}

	// Determine which direction(s) we need to traverse
	int dirX = (nx < 0) ? 0 : ((nx > 0) ? 1 : -1); // -1 means no X traversal
	int dirY = (ny < 0) ? 2 : ((ny > 0) ? 3 : -1); // -1 means no Y traversal
	int dirZ = (nz < 0) ? 4 : ((nz > 0) ? 5 : -1); // -1 means no Z traversal

	const Chunk* neighbor = this;

	// Traverse in X direction first
	if (dirX != -1)
	{
		neighbor = neighbor->neighbors[dirX];
		if (!neighbor) return 15 << 4;
	}

	// Then traverse in Y direction
	if (dirY != -1)
	{
		neighbor = neighbor->neighbors[dirY];
		if (!neighbor) return 15 << 4;
	}

	// Finally traverse in Z direction
	if (dirZ != -1)
	{
		neighbor = neighbor->neighbors[dirZ];
		if (!neighbor) return 15 << 4;
	}

	// Get local coordinates in the final neighbor chunk
	int localX = x & CHUNK_LOWER_BITS_MASK;
	int localY = y & CHUNK_LOWER_BITS_MASK;
	int localZ = z & CHUNK_LOWER_BITS_MASK;

	return neighbor->getLight_inBoundaries(localX, localY, localZ);
}

void Chunk::setLight_inBoundaries(int x, int y, int z, uint8_t lightValue)
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	light[getIndex(x, y, z)] = lightValue;
}

std::pair<Block, uint8_t> Chunk::getBlockAndLight_inBoundaries(int x, int y, int z) const
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	return std::make_pair(blocks[getIndex(x, y, z)], light[getIndex(x, y, z)]);
}

std::pair<Block, uint8_t> Chunk::getBlockAndLight_checkSideNeighbor(int x, int y, int z, int side) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx == 0 && ny == 0 && nz == 0)
	{
		return std::make_pair(blocks[getIndex(x, y, z)], light[getIndex(x, y, z)]);
	}

	const Chunk* neighbor = neighbors[side];
	if (neighbor)
	{
		return neighbor->getBlockAndLight_inBoundaries(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
	}
	return std::make_pair(Block::Air, 15 << 4);
}

std::pair<Block, uint8_t> Chunk::getBlockAndLight_checkNeighborsTraverse(int x, int y, int z) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	// If within current chunk bounds
	if (nx == 0 && ny == 0 && nz == 0)
	{
		return std::make_pair(blocks[getIndex(x, y, z)], light[getIndex(x, y, z)]);
	}

	// Determine which direction(s) we need to traverse
	int dirX = (nx < 0) ? 0 : ((nx > 0) ? 1 : -1); // -1 means no X traversal
	int dirY = (ny < 0) ? 2 : ((ny > 0) ? 3 : -1); // -1 means no Y traversal
	int dirZ = (nz < 0) ? 4 : ((nz > 0) ? 5 : -1); // -1 means no Z traversal

	const Chunk* neighbor = this;

	// Traverse in X direction first
	if (dirX != -1)
	{
		neighbor = neighbor->neighbors[dirX];
		if (!neighbor) return std::make_pair(Block::Air, 15 << 4);
	}

	// Then traverse in Y direction
	if (dirY != -1)
	{
		neighbor = neighbor->neighbors[dirY];
		if (!neighbor) return std::make_pair(Block::Air, 15 << 4);
	}

	// Finally traverse in Z direction
	if (dirZ != -1)
	{
		neighbor = neighbor->neighbors[dirZ];
		if (!neighbor) return std::make_pair(Block::Air, 15 << 4);
	}

	// Get local coordinates in the final neighbor chunk
	int localX = x & CHUNK_LOWER_BITS_MASK;
	int localY = y & CHUNK_LOWER_BITS_MASK;
	int localZ = z & CHUNK_LOWER_BITS_MASK;

	return neighbor->getBlockAndLight_inBoundaries(localX, localY, localZ);
}

void Chunk::addNodeToLightQueue(int x, int y, int z, uint8_t lightLevel, int8_t propagationSide)
{
	std::lock_guard<std::mutex> lock(lightMutex);
	lightNodeContainer.emplace_back(x, y, z, lightLevel, propagationSide);
}

void Chunk::calculateVertexAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, uint8_t centerLight, uint8_t side1Light, uint8_t side2Light, uint8_t cornerLight, bool side1Solid, bool side2Solid, bool cornerSolid) const
{
	unsigned int blockLightSum = centerLight & 15;
	unsigned int skyLightSum = (centerLight >> 4) & 15;
	unsigned int count = 1;

	if (side1Solid && side2Solid)
	{
		ao = 0;
		light = (blockLightSum & 15) | ((skyLightSum & 15) << 4);
		return;
	}

	if (!side1Solid)
	{
		blockLightSum += side1Light & 15;
		skyLightSum += (side1Light >> 4) & 15;
		count++;
	}

	if (!side2Solid)
	{
		blockLightSum += side2Light & 15;
		skyLightSum += (side2Light >> 4) & 15;
		count++;
	}

	if (!side1Solid && !side2Solid && !cornerSolid)
	{
		blockLightSum += cornerLight & 15;
		skyLightSum += (cornerLight >> 4) & 15;
		count++;
	}

	unsigned int avgBlockLight = blockLightSum / count;
	unsigned int avgSkyLight = skyLightSum / count;

	assert(avgBlockLight >= 0 && avgBlockLight <= 15);
	assert(avgSkyLight >= 0 && avgSkyLight <= 15);

	ao = 3 - (side1Solid + side2Solid + cornerSolid);
	light = (avgBlockLight & 15) | ((avgSkyLight & 15) << 4);
}

void Chunk::calculateFaceAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, int x, int y, int z, int normal, uint8_t centerFaceLight) const
{
	// For each face normal, we need to check 8 neighbors around the face
	// The AO calculation depends on which direction the face is facing

	std::pair<Block, uint8_t> data[8];
	bool n[8]; // 8 neighbors around the face

	unsigned int ao0, ao1, ao2, ao3;
	unsigned int light0, light1, light2, light3;

	switch (normal)
	{
	case 0: // -X face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z + 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y, z - 1);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x - 1, y, z + 1);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z - 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		break;
	case 1: // +X face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z + 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x + 1, y, z - 1);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y, z + 1);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z - 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		break;
	case 2: // -Y face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x, y - 1, z - 1);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z - 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z + 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x, y - 1, z + 1);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		break;
	case 3: // +Y face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x, y + 1, z - 1);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z - 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z + 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x, y + 1, z + 1);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[4].second, data[1].second, data[2].second, n[4], n[1], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[3].second, data[1].second, data[0].second, n[3], n[1], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[3].second, data[6].second, data[5].second, n[3], n[6], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[4].second, data[6].second, data[7].second, n[4], n[6], n[7]);
		break;
	case 4: // -Z face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x, y - 1, z - 1);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z - 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y, z - 1);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y, z - 1);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z - 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x, y + 1, z - 1);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z - 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		break;
	case 5: // +Z face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z + 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x, y - 1, z + 1);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z + 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y, z + 1);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y, z + 1);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z + 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x, y + 1, z + 1);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		break;
	}

	//
	ao = ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6);
	light = light0 | (light1 << 8) | (light2 << 16) | (light3 << 24);
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
	}

	//
	for (MeshData* chunkMesh : pendingMeshUploads)
	{
		chunkMesh->processingFence.stopProcessing();
	}
	pendingMeshUploads.clear();
}

//============================================================================
//LightNode

LightNode::LightNode(int x, int y, int z, uint8_t lightLevel, int8_t propagationSide) :
	x(x), y(y), z(z), lightLevel(lightLevel), propagationSide(propagationSide)
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