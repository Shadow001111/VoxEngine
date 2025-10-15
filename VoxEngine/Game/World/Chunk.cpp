#include "Chunk.h"
#include "Chunk/TerrainGenerator.h"

#include "Core/Profiler.h"

#include <cassert>
#include <vector>
#include <iostream>

std::mutex Chunk::meshUploadMutex;
std::vector<MeshData*> Chunk::pendingMeshUploads;
BlockTextureIDDatabase Chunk::blockTextureDatabase;

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
	position = Int3(x, y, z);

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

	assert(!meshData.processingFence.isProcessing());
	meshData.ready = false;
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
	meshData.instances.clear();

	// Release chunk column data
	if (isLoadedChunkColumnData.load(std::memory_order_acquire))
	{
		isLoadedChunkColumnData.store(false, std::memory_order_release);
		TerrainGenerator::getInstance().unloadChunkColumnData(position.x, position.z);
	}

	//
	{
		std::lock_guard<std::mutex> lock(lightMutex);
		while (!lightQueue.empty())
		{
			lightQueue.pop();
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

	// Cave mask
	// TODO: Chunk masks shouldn't be computed for chunks above terrain.
	bool caveMask[CHUNK_VOLUME];
	TerrainGenerator::getInstance().computeCaveMask(caveMask, position.x, position.y, position.z);

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
						block = caveMask[index] ? Block::Air : Block::Stone;
					}

					blocks[index] = block;
				}
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
	std::queue<LightNode> localLightQueue;
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

					uint8_t emission = currentBlockData->lightEmission;
					if (emission == 0)
					{
						continue;
					}

					if (currentBlockData->hasTransparentFaces)
					{
						localLightQueue.emplace(x, y, z, emission, -1);
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
							localLightQueue.emplace(nx, ny, nz, emission, i);
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

		while (!localLightQueue.empty())
		{
			auto data = localLightQueue.front();
			localLightQueue.pop();

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

			uint8_t lightAbsorption = blockData->lightAbsorption;
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
					localLightQueue.emplace(nx, ny, nz, newBlockLight, i);
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

	meshData.ready = false;

	{
		PROFILE_SCOPE("Build chunk mesh", ProfileCategory::ChunkMesh);

		// Collect visible faces
		std::vector<BlockFaceInstance> instances[6];
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					Block block = getBlock_inBoundaries(x, y, z);
					// TODO: Add 'hasFaces' to BlockData
					if (block == Block::Air)
					{
						continue;
					}

					// TODO: Maybe should copy?
					const auto& textureIDs = blockTextureDatabase.getBlockTextureIDs(block);

					// I tried to do a loop, but it doubles the execution time

					// -X
					std::pair<Block, uint8_t> blockAndLight = getBlockAndLight_checkSideNeighbor(x - 1, y, z, 0);
					const BlockData* blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->hasTransparentFaces && block != blockAndLight.first)
					{
						int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 0, blockAndLight.second);
						instances[0].emplace_back(x, y, z, 0, ao, textureIDs.ids[0], light);
					}

					// +X
					blockAndLight = getBlockAndLight_checkSideNeighbor(x + 1, y, z, 1);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->hasTransparentFaces && block != blockAndLight.first)
					{
						int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 1, blockAndLight.second);
						instances[1].emplace_back(x, y, z, 1, ao, textureIDs.ids[1], light);
					}

					// -Y
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y - 1, z, 2);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->hasTransparentFaces && block != blockAndLight.first)
					{
						int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 2, blockAndLight.second);
						instances[2].emplace_back(x, y, z, 2, ao, textureIDs.ids[2], light);
					}

					// +Y
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y + 1, z, 3);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->hasTransparentFaces && block != blockAndLight.first)
					{
						int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 3, blockAndLight.second);
						instances[3].emplace_back(x, y, z, 3, ao, textureIDs.ids[3], light);
					}

					// -Z
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y, z - 1, 4);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->hasTransparentFaces && block != blockAndLight.first)
					{
						int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 4, blockAndLight.second);
						instances[4].emplace_back(x, y, z, 4, ao, textureIDs.ids[4], light);
					}

					// +Z
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y, z + 1, 5);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->hasTransparentFaces && block != blockAndLight.first)
					{
						int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 5, blockAndLight.second);
						instances[5].emplace_back(x, y, z, 5, ao, textureIDs.ids[5], light);
					}
				}
			}
		}

		// Combine instances vectors
		size_t faceCountSum = 0;
		for (int i = 0; i < 6; i++)
		{
			size_t faceCount = instances[i].size();
			meshData.faceCount[i] = faceCount;
			faceCountSum += faceCount;
		}

		assert(!meshData.processingFence.isProcessing());

		meshData.instances.clear();
		meshData.instances.reserve(faceCountSum);
		for (int i = 0; i < 6; i++)
		{
			const auto& vectorToInsert = instances[i];
			meshData.instances.insert(meshData.instances.end(), vectorToInsert.begin(), vectorToInsert.end());
		}
	}

	if (!isLoadedInWorld.load(std::memory_order_acquire))
	{
		return;
	}

	{
		// TODO: Since we are gonna keep mesh in chunk all time in the future anyway, why not collecting meshes in main thread instead of sending them?
		// Possibly will take less time, since no mutex locks
		PROFILE_SCOPE("Send chunk meshes to main thread", ProfileCategory::ChunkMesh);

		// Queue mesh for GPU upload on main thread
		std::lock_guard<std::mutex> lock(meshUploadMutex);
		pendingMeshUploads.push_back( &meshData );
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
	std::queue<LightNode> localLightQueue;
	{
		std::lock_guard<std::mutex> lock(lightMutex);
		if (lightQueue.empty())
		{
			return;
		}
		localLightQueue.swap(lightQueue);
	}

	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };

	bool lightChanged = false;

	// Process light propagation
	while (!localLightQueue.empty())
	{
		auto data = localLightQueue.front();
		localLightQueue.pop();

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

		uint8_t lightAbsorption = blockData->lightAbsorption;
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
				localLightQueue.emplace(nx, ny, nz, newBlockLight, i);
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
	return !lightQueue.empty();
}

void Chunk::render() const
{
	// TODO: Old mesh data can be seen when loading!
	// Checking twice to be sure
	if (!canBeRendered())
	{
		return;
	}

	size_t faceCount = meshData.getFaceCountSum();
	meshData.bindVAO();
	glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, faceCount);
}

bool Chunk::canBeRendered() const
{
	size_t faceCount = meshData.getFaceCountSum();
	return
		getState() == State::Ready &&
		meshData.ready &&
		faceCount > 0 &&
		faceCount <= meshData.getFaceCapacity() &&
		!processingFence.isProcessing();// &&
		//!meshData.processingFence.isProcessing();
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
	lightQueue.emplace(x, y, z, lightLevel, propagationSide);
}

void Chunk::calculateVertexAmbientOcclusionAndLight(int& ao, int& light, uint8_t centerLight, uint8_t side1Light, uint8_t side2Light, uint8_t cornerLight, bool side1Solid, bool side2Solid, bool cornerSolid) const
{
	int blockLightSum = centerLight & 15;
	int skyLightSum = (centerLight >> 4) & 15;
	int count = 1;

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

	int avgBlockLight = blockLightSum / count;
	int avgSkyLight = skyLightSum / count;

	assert(avgBlockLight >= 0 && avgBlockLight <= 15);
	assert(avgSkyLight >= 0 && avgSkyLight <= 15);

	ao = 3 - (side1Solid + side2Solid + cornerSolid);
	light = (avgBlockLight & 15) | ((avgSkyLight & 15) << 4);
}

void Chunk::calculateFaceAmbientOcclusionAndLight(int& ao, int& light, int x, int y, int z, int normal, uint8_t centerFaceLight) const
{
	// For each face normal, we need to check 8 neighbors around the face
	// The AO calculation depends on which direction the face is facing

	std::pair<Block, uint8_t> data[8];
	bool n[8]; // 8 neighbors around the face

	int ao0, ao1, ao2, ao3;
	int light0, light1, light2, light3;

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
			n[i] = !BlockDataBase::getBlockData(data[i].first)->hasTransparentFaces;
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
			n[i] = !BlockDataBase::getBlockData(data[i].first)->hasTransparentFaces;
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
			n[i] = !BlockDataBase::getBlockData(data[i].first)->hasTransparentFaces;
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
			n[i] = !BlockDataBase::getBlockData(data[i].first)->hasTransparentFaces;
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
			n[i] = !BlockDataBase::getBlockData(data[i].first)->hasTransparentFaces;
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
			n[i] = !BlockDataBase::getBlockData(data[i].first)->hasTransparentFaces;
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

Int3 Chunk::getPosition() const
{
	return position;
}

size_t Chunk::getFaceCount() const
{
	return meshData.getFaceCountSum();
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
	PROFILE_SCOPE("Send meshes to GPU", ProfileCategory::ChunkMesh);

	// Get all pending uploads
	std::vector<MeshData*> uploads;
	{
		std::lock_guard<std::mutex> lock(meshUploadMutex);

		if (pendingMeshUploads.empty())
		{
			return;
		}

		uploads.swap(pendingMeshUploads);
	}

	// Process each upload
	for (auto& meshData : uploads)
	{
		ScopedProcessingFence scopedFence(meshData->processingFence);

		meshData->bindInstanceVBO();

		// Allocating data for buffer
		size_t faceCountSum = meshData->getFaceCountSum();
		meshData->allocateMemoryForBuffer(faceCountSum);

		// Write to buffer
		glBufferSubData(GL_ARRAY_BUFFER, 0, faceCountSum * sizeof(BlockFaceInstance), meshData->instances.data());

		meshData->ready = true;

		meshData->instances.clear();
	}
}

//============================================================================

LightNode::LightNode(int x, int y, int z, uint8_t lightLevel, int8_t propagationSide) :
	x(x), y(y), z(z), lightLevel(lightLevel), propagationSide(propagationSide)
{
}
