#include "Chunk.h"
#include "Chunk/TerrainGenerator.h"

#include "Core/Profiler.h"

#include <cassert>
#include <vector>
#include <iostream>

std::mutex Chunk::meshUploadMutex;
std::vector<MeshData*> Chunk::pendingMeshUploads;
BlockTextureIDDatabase Chunk::blockTextureDatabase;


size_t Chunk::getIndex(int x, int y, int z)
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

bool Chunk::operator==(const Chunk& other) const
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
}

// Fills 'blocks' array
void Chunk::buildBlocks()
{
	Profiler::beginProfile("Chunk build blocks: wait", ProfileCategory::ChunkBlocks);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	if (!getIsLoadedInWorld())
	{
		return;
	}

	if (areBlocksBuilt.load(std::memory_order_acquire))
	{
		return;
	}

	if (isLoadedChunkColumnData.load(std::memory_order_acquire))
	{
		return;
	}

	const ChunkColumnData* chunkColumnData = TerrainGenerator::getInstance().loadChunkColumnData(position.x, position.z);
	const int* heightMap = chunkColumnData->heightMapRead();
	isLoadedChunkColumnData.store(true, std::memory_order_release);

	{
		PROFILE_SCOPE("Chunk build blocks", ProfileCategory::ChunkBlocks);
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				const int globalHeight = heightMap[z + x * CHUNK_SIZE];

				for (int y = 0; y < CHUNK_SIZE; y++)
				{
					int worldY = position.y * CHUNK_SIZE + y;
					if (worldY <= globalHeight)
					{
						blocks[getIndex(x, y, z)] = Block::GrassBlock;
					}
					else
					{
						blocks[getIndex(x, y, z)] = Block::Air;
					}
				}
			}
		}
	}

	areBlocksBuilt.store(true, std::memory_order_release);
}

void Chunk::buildMesh()
{
	Profiler::beginProfile("Build chunk mesh: wait", ProfileCategory::ChunkMesh);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	if (!getIsLoadedInWorld())
	{
		return;
	}

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
					if (block == Block::Air)
					{
						continue;
					}

					// TODO: Maybe should copy?
					const auto& textureIDs = blockTextureDatabase.getBlockTextureIDs(block);

					// TODO: Maybe use a loop for simplifying code?
					// -X
					if (getBlock_checkSideNeighbor(x - 1, y, z, 0) == Block::Air)
					{
						int ao = calculateFaceAO(x, y, z, 0);
						instances[0].emplace_back(x, y, z, 0, ao, textureIDs.ids[0]);
					}
					// +X
					if (getBlock_checkSideNeighbor(x + 1, y, z, 1) == Block::Air)
					{
						int ao = calculateFaceAO(x, y, z, 1);
						instances[1].emplace_back(x, y, z, 1, ao, textureIDs.ids[1]);
					}
					// -Y
					if (getBlock_checkSideNeighbor(x, y - 1, z, 2) == Block::Air)
					{
						int ao = calculateFaceAO(x, y, z, 2);
						instances[2].emplace_back(x, y, z, 2, ao, textureIDs.ids[2]);
					}
					// +Y
					if (getBlock_checkSideNeighbor(x, y + 1, z, 3) == Block::Air)
					{
						int ao = calculateFaceAO(x, y, z, 3);
						instances[3].emplace_back(x, y, z, 3, ao, textureIDs.ids[3]);
					}
					// -Z
					if (getBlock_checkSideNeighbor(x, y, z - 1, 4) == Block::Air)
					{
						int ao = calculateFaceAO(x, y, z, 4);
						instances[4].emplace_back(x, y, z, 4, ao, textureIDs.ids[4]);
					}
					// +Z
					if (getBlock_checkSideNeighbor(x, y, z + 1, 5) == Block::Air)
					{
						int ao = calculateFaceAO(x, y, z, 5);
						instances[5].emplace_back(x, y, z, 5, ao, textureIDs.ids[5]);
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
	
	//std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Slowing down for testing

	if (!getIsLoadedInWorld())
	{
		return;
	}

	{
		// TODO: Since we are gonna keep mesh in chunk all time in the future anyway, why not collecting meshes in main thread instead of sending them?
		// Possibly will take less time, since no mutex locks
		PROFILE_SCOPE("Send chunk mesh to main thread", ProfileCategory::ChunkMesh);

		// Queue mesh for GPU upload on main thread
		std::lock_guard<std::mutex> lock(meshUploadMutex);
		pendingMeshUploads.push_back( &meshData );
	}
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

// Function checks neighbors, if out of boundaries. Handles only 6 neighbors.
Block Chunk::getBlock_checkNeighbors(int x, int y, int z) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx == 0 && ny == 0 && nz == 0)
	{
		return blocks[getIndex(x, y, z)];
	}

	const Chunk* neighbor = nullptr;
	if (nx < 0) neighbor = neighbors[0]; // -X
	else if (nx > 0) neighbor = neighbors[1]; // +X
	else if (ny < 0) neighbor = neighbors[2]; // -Y
	else if (ny > 0) neighbor = neighbors[3]; // +Y
	else if (nz < 0) neighbor = neighbors[4]; // -Z
	else if (nz > 0) neighbor = neighbors[5]; // +Z

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

int Chunk::calculateVertexAO(bool side1, bool side2, bool corner) const
{
	if (side1 && side2)
	{
		return 0; // Darkest
	}
	return 3 - (side1 + side2 + corner); // 3, 2, or 1
}

int Chunk::calculateFaceAO(int x, int y, int z, int normal) const
{
	// For each face normal, we need to check 8 neighbors around the face
	// The AO calculation depends on which direction the face is facing

	bool n[8]; // 8 neighbors around the face
	int ao0, ao1, ao2, ao3;

	switch (normal)
	{
	case 0: // -X face
		n[0] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y - 1, z - 1);
		n[1] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y - 1, z);
		n[2] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y - 1, z + 1);
		n[3] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y, z - 1);
		n[4] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y, z + 1);
		n[5] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y + 1, z - 1);
		n[6] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y + 1, z);
		n[7] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y + 1, z + 1);

		ao0 =  calculateVertexAO(n[1], n[3], n[0]);
		ao1 =  calculateVertexAO(n[1], n[4], n[2]);
		ao2 =  calculateVertexAO(n[6], n[4], n[7]);
		ao3 =  calculateVertexAO(n[6], n[3], n[5]);
		break;
	case 1: // +X face
		n[0] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y - 1, z - 1);
		n[1] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y - 1, z);
		n[2] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y - 1, z + 1);
		n[3] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y, z - 1);
		n[4] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y, z + 1);
		n[5] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y + 1, z - 1);
		n[6] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y + 1, z);
		n[7] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y + 1, z + 1);

		ao0 = calculateVertexAO(n[1], n[4], n[2]);
		ao1 = calculateVertexAO(n[1], n[3], n[0]);
		ao2 = calculateVertexAO(n[6], n[3], n[5]);
		ao3 = calculateVertexAO(n[6], n[4], n[7]);
		break;
	case 2: // -Y face
		n[0] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y - 1, z - 1);
		n[1] = Block::Air != getBlock_checkNeighborsTraverse(x, y - 1, z - 1);
		n[2] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y - 1, z - 1);
		n[3] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y - 1, z);
		n[4] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y - 1, z);
		n[5] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y - 1, z + 1);
		n[6] = Block::Air != getBlock_checkNeighborsTraverse(x, y - 1, z + 1);
		n[7] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y - 1, z + 1);

		ao0 = calculateVertexAO(n[1], n[4], n[2]);
		ao1 = calculateVertexAO(n[1], n[3], n[0]);
		ao2 = calculateVertexAO(n[6], n[3], n[5]);
		ao3 = calculateVertexAO(n[6], n[4], n[7]);
		break;
	case 3: // +Y face
		n[0] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y + 1, z - 1);
		n[1] = Block::Air != getBlock_checkNeighborsTraverse(x, y + 1, z - 1);
		n[2] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y + 1, z - 1);
		n[3] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y + 1, z);
		n[4] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y + 1, z);
		n[5] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y + 1, z + 1);
		n[6] = Block::Air != getBlock_checkNeighborsTraverse(x, y + 1, z + 1);
		n[7] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y + 1, z + 1);

		ao0 = calculateVertexAO(n[4], n[1], n[2]);
		ao1 = calculateVertexAO(n[3], n[1], n[0]);
		ao2 = calculateVertexAO(n[3], n[6], n[5]);
		ao3 = calculateVertexAO(n[4], n[6], n[7]);
		break;
	case 4: // -Z face
		n[0] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y - 1, z - 1);
		n[1] = Block::Air != getBlock_checkNeighborsTraverse(x, y - 1, z - 1);
		n[2] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y - 1, z - 1);
		n[3] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y, z - 1);
		n[4] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y, z - 1);
		n[5] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y + 1, z - 1);
		n[6] = Block::Air != getBlock_checkNeighborsTraverse(x, y + 1, z - 1);
		n[7] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y + 1, z - 1);

		ao0 = calculateVertexAO(n[1], n[4], n[2]);
		ao1 = calculateVertexAO(n[1], n[3], n[0]);
		ao2 = calculateVertexAO(n[6], n[3], n[5]);
		ao3 = calculateVertexAO(n[6], n[4], n[7]);
		break;
	case 5: // +Z face
		n[0] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y - 1, z + 1);
		n[1] = Block::Air != getBlock_checkNeighborsTraverse(x, y - 1, z + 1);
		n[2] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y - 1, z + 1);
		n[3] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y, z + 1);
		n[4] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y, z + 1);
		n[5] = Block::Air != getBlock_checkNeighborsTraverse(x - 1, y + 1, z + 1);
		n[6] = Block::Air != getBlock_checkNeighborsTraverse(x, y + 1, z + 1);
		n[7] = Block::Air != getBlock_checkNeighborsTraverse(x + 1, y + 1, z + 1);

		ao0 = calculateVertexAO(n[1], n[3], n[0]);
		ao1 = calculateVertexAO(n[1], n[4], n[2]);
		ao2 = calculateVertexAO(n[6], n[4], n[7]);
		ao3 = calculateVertexAO(n[6], n[3], n[5]);
		break;
	}
	return ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6);
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
