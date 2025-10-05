#include "Chunk.h"

#include "Vec2.h"
#include "Profiler.h"

#include <cassert>
#include <vector>
#include "TerrainGenerator.h"

//============================================================================
//BlockFaceInstance

BlockFaceInstance::BlockFaceInstance(int x, int y, int z, int normal, int ao) : data(0)
{
	// Coords 12 bits
	data |= (x & 15);
	data |= (y & 15) << 4;
	data |= (z & 15) << 8;

	// Normal 3 bits
	data |= (normal & 7) << 12;

	// Ambient occlusion 8 bits
	data |= (ao & 255) << 15;
}

//============================================================================
// PendingMeshUpload

Chunk::PendingMeshUpload::PendingMeshUpload(std::vector<BlockFaceInstance>&& instances, GLuint instanceVBO, Chunk* chunk) :
	instances(std::move(instances)), instanceVBO(instanceVBO), chunk(chunk)
{
}

//============================================================================
// Chunk

std::mutex Chunk::meshUploadMutex;
std::vector<Chunk::PendingMeshUpload> Chunk::pendingMeshUploads;


size_t Chunk::getIndex(int x, int y, int z)
{
	return (x << 8) | (y << 4) | z;
}

Chunk::Chunk() :
	position(0, 0, 0),
	vao(0), vbo(0), instanceVBO(0), faceCount(0), faceCapacity(0),
	beingProcessed(false)
{
	// Create buffers once
	Vec2 vertices[4] = // CCW order
	{
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f }
	};

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &instanceVBO);

	// Bind VAO
	glBindVertexArray(vao);

	// Vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), (void*)0);

	// Instance buffer
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 1, GL_INT, sizeof(BlockFaceInstance), (void*)0); // integer attribute
	glVertexAttribDivisor(1, 1); // advance per instance

	// Neighbours are null
	for (int i = 0; i < 6; i++)
	{
		neighbors[i] = nullptr;
	}
}

Chunk::~Chunk()
{
	destroy(); // Just in case

	// Delete buffers
	if (instanceVBO)
	{
		glDeleteBuffers(1, &instanceVBO);
		instanceVBO = 0;
	}
	if (vbo)
	{
		glDeleteBuffers(1, &vbo);
		vbo = 0;
	}
	if (vao)
	{
		glDeleteVertexArrays(1, &vao);
		vao = 0;
	}

	faceCapacity = 0;
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

	// Clear blocks
	// TODO: This in unecessary, since buildBlocks fills whole array
	/*for (int i = 0; i < CHUNK_VOLUME; i++)
	{
		blocks[i] = Block::Air;
	}*/

	// Set instance count to 0
	faceCount = 0;

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

	//
	loadedChunkColumnData = false;

	// Reset state
	setState(State::NeedsBlocks);
}

// Cleans up resources
void Chunk::destroy()
{
	// Set instance count to 0
	faceCount = 0;

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

	// Release chunk column data
	if (loadedChunkColumnData)
	{
		TerrainGenerator::getInstance().releaseChunkColumnData(position.x, position.z);
	}

	// State can be not reset, because who cares?
}

// Fills 'blocks' array
void Chunk::buildBlocks()
{
	assert(!isBeingProcessed());
	setIsBeingProcessed(true);

	auto chunkColumnData = TerrainGenerator::getInstance().loadChunkColumnData(position.x, position.z);
	const int* heightMap = chunkColumnData->heightMap;
	loadedChunkColumnData = true;

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
					blocks[getIndex(x, y, z)] = Block::Solid;
				}
				else
				{
					blocks[getIndex(x, y, z)] = Block::Air;
				}
			}
		}
	}

	assert(isBeingProcessed());
	setIsBeingProcessed(false);
}

void Chunk::buildMesh()
{
	assert(!isBeingProcessed());
	setIsBeingProcessed(true);

	std::vector<BlockFaceInstance> mesh;

	// Collect visible faces
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

				// -X
				if (getBlock_checkSideNeighbor(x - 1, y, z, 0) == Block::Air)
				{
					int ao = calculateFaceAO(x, y, z, 0);
					mesh.emplace_back(x, y, z, 0, ao);
				}
				// +X
				if (getBlock_checkSideNeighbor(x + 1, y, z, 1) == Block::Air)
				{
					int ao = calculateFaceAO(x, y, z, 1);
					mesh.emplace_back(x, y, z, 1, ao);
				}
				// -Y
				if (getBlock_checkSideNeighbor(x, y - 1, z, 2) == Block::Air)
				{
					int ao = calculateFaceAO(x, y, z, 2);
					mesh.emplace_back(x, y, z, 2, ao);
				}
				// +Y
				if (getBlock_checkSideNeighbor(x, y + 1, z, 3) == Block::Air)
				{
					int ao = calculateFaceAO(x, y, z, 3);
					mesh.emplace_back(x, y, z, 3, ao);
				}
				// -Z
				if (getBlock_checkSideNeighbor(x, y, z - 1, 4) == Block::Air)
				{
					int ao = calculateFaceAO(x, y, z, 4);
					mesh.emplace_back(x, y, z, 4, ao);
				}
				// +Z
				if (getBlock_checkSideNeighbor(x, y, z + 1, 5) == Block::Air)
				{
					int ao = calculateFaceAO(x, y, z, 5);
					mesh.emplace_back(x, y, z, 5, ao);
				}
			}
		}
	}

	// Queue mesh for GPU upload on main thread
	{
		std::lock_guard<std::mutex> lock(meshUploadMutex);
		pendingMeshUploads.emplace_back(
			std::move(mesh),
			instanceVBO,
			this
			);
	}

	assert(isBeingProcessed());
	setIsBeingProcessed(false);
}

void Chunk::render() const
{
	if (faceCount == 0) return;
	glBindVertexArray(vao);
	glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, faceCount);
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
	else
	{
		return Block::Air;
	}
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
	else
	{
		return Block::Air;
	}
}

// Function checks neighbors, if out of boundaries. Handles diagonal neighbors too.
// TODO: Consider storing 26 neighbors in Chunk
Block Chunk::getBlock_checkNeighborsTraverse(int x, int y, int z) const
{
	// TODO: Traverse can fail and success depending on traversal order. I think it won't be noticable on normal render distance.

	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	// If within current chunk bounds
	if (nx == 0 && ny == 0 && nz == 0)
	{
		return blocks[getIndex(x, y, z)];
	}

	// Determine which direction(s) we need to traverse
	int dirX = (nx < 0) ? 0 : (nx > 0) ? 1 : -1; // -1 means no X traversal
	int dirY = (ny < 0) ? 2 : (ny > 0) ? 3 : -1; // -1 means no Y traversal
	int dirZ = (nz < 0) ? 4 : (nz > 0) ? 5 : -1; // -1 means no Z traversal

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

Chunk::State Chunk::getState() const
{
	return state.load(std::memory_order_acquire);
}

void Chunk::setState(State newState)
{
	state.store(newState, std::memory_order_release);
}

bool Chunk::isBeingProcessed() const
{
	return beingProcessed.load(std::memory_order_acquire);
}

void Chunk::setIsBeingProcessed(bool value)
{
	beingProcessed.store(value, std::memory_order_release);
}

void Chunk::sendMeshesToGPU()
{
	PROFILE_SCOPE("Send meshes to GPU");

	// Get all pending uploads
	std::vector<PendingMeshUpload> uploads;
	{
		std::lock_guard<std::mutex> lock(meshUploadMutex);
		uploads.swap(pendingMeshUploads);
	}

	// Process each upload
	for (auto& upload : uploads)
	{
		const auto& mesh = upload.instances;
		GLuint vbo = upload.instanceVBO;
		Chunk* chunk = upload.chunk;

		// Upload to GPU
		chunk->faceCount = mesh.size();

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		if (chunk->faceCount > chunk->faceCapacity)
		{
			chunk->faceCapacity = chunk->faceCount;
			glBufferData(GL_ARRAY_BUFFER, chunk->faceCount * sizeof(BlockFaceInstance),
				mesh.data(), GL_STATIC_DRAW);
		}
		else
		{
			glBufferSubData(GL_ARRAY_BUFFER, 0, chunk->faceCount * sizeof(BlockFaceInstance),
				mesh.data());
		}
	}
}

uint32_t Chunk::getFaceCount() const
{
	return faceCount;
}

uint32_t Chunk::getFaceCapacity() const
{
	return faceCapacity;
}

//============================================================================
