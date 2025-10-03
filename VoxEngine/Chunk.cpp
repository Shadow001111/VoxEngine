#include "Chunk.h"

#include "Vec2.h"
#include "Profiler.h"

#include <cassert>
#include <vector>
#include <iostream>

//============================================================================
//BlockFaceInstance

struct BlockFaceInstance
{
	int32_t data;

	BlockFaceInstance(int x, int y, int z, int normal) : data(0)
	{
		// Coords 12 bits
		data |= (x & 15);
		data |= (y & 15) << 4;
		data |= (z & 15) << 8;

		// Normal 3 bits
		data |= (normal & 7) << 12;
	}
};

//============================================================================
// Chunk

size_t Chunk::getIndex(int x, int y, int z)
{
	return (x << 8) | (y << 4) | z;
}

Chunk::Chunk() :
	position(0, 0, 0),
	vao(0), vbo(0), instanceVBO(0), faceCount(0), faceCapacity(0)
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
	PROFILE_SCOPE("Chunk init");

	// Set position
	position = Int3(x, y, z);

	// Clear blocks
	for (int i = 0; i < CHUNK_VOLUME; i++)
	{
		blocks[i] = Block::Air;
	}

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
}

// Fills 'blocks' array
void Chunk::buildBlocks()
{
	PROFILE_SCOPE("Chunk build blocks");

	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int z = 0; z < CHUNK_SIZE; z++)
		{
			int globalHeight = 0; // Flat terrain

			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				int worldY = position.y * CHUNK_SIZE + y;
				if (worldY < globalHeight)
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
}

void Chunk::buildMesh()
{
	// TODO: Mesh allocates each function call, maybe it should be a member and reused?
	static thread_local std::vector<BlockFaceInstance> mesh;

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
				if (getBlock_checkNeighbors(x - 1, y, z) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 0);
				}
				// +X
				if (getBlock_checkNeighbors(x + 1, y, z) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 1);
				}
				// -Y
				if (getBlock_checkNeighbors(x, y - 1, z) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 2);
				}
				// +Y
				if (getBlock_checkNeighbors(x, y + 1, z) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 3);
				}
				// -Z
				if (getBlock_checkNeighbors(x, y, z - 1) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 4);
				}
				// +Z
				if (getBlock_checkNeighbors(x, y, z + 1) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 5);
				}
			}
		}
	}

	// Upload to GPU
	{
		// TODO: Maybe have a single VBO/VAO for all chunks, since they use the same vertices?

		// Instance buffer
		faceCount = mesh.size();

		glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
		if (faceCount > faceCapacity)
		{
			faceCapacity = faceCount;
			glBufferData(GL_ARRAY_BUFFER, faceCount * sizeof(BlockFaceInstance), mesh.data(), GL_STATIC_DRAW);
		}
		else
		{
			glBufferSubData(GL_ARRAY_BUFFER, 0, faceCount * sizeof(BlockFaceInstance), mesh.data());
		}

		// Clear mesh for next build
		mesh.clear();
	}
}

void Chunk::render() const
{
	if (faceCount == 0) return;
	glBindVertexArray(vao);
	glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, faceCount);
}

// Function doesn't check for bounsaries, it trusts the caller. On debug mode, it asserts.
Block Chunk::getBlock_inBoundaries(int x, int y, int z) const
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	return blocks[getIndex(x, y, z)];
}

// Function checks neighbors, if out of boundaries. Neighbours are considered Air for now.
Block Chunk::getBlock_checkNeighbors(int x, int y, int z) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx != 0 || ny != 0 || nz != 0)
	{
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
			return Block::Air; // No neighbor means air
		}
	}

	return blocks[getIndex(x, y, z)];
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
	return faceCount;
}

size_t Chunk::getFaceCapacity() const
{
	return faceCapacity;
}

//============================================================================
// ChunkHash

size_t ChunkHash::operator()(const Chunk& chunk) const
{
	// 21 bits for each coordinate should be enough (-1048576 to 1048575), because chunks won't be generated that far from each other... unless multiplayer.
	Int3 pos = chunk.getPosition();
	return (size_t)pos.x | ((size_t)pos.y << 21) | ((size_t)pos.z << 42);
}

size_t ChunkHash::operator()(const Chunk* chunk) const
{
	Int3 pos = chunk->getPosition();
	return (size_t)pos.x | ((size_t)pos.y << 21) | ((size_t)pos.z << 42);
}

size_t ChunkHash::operator()(const std::unique_ptr<Chunk>& chunk) const
{
	Int3 pos = chunk->getPosition();
	return (size_t)pos.x | ((size_t)pos.y << 21) | ((size_t)pos.z << 42);
}

size_t ChunkHash::operator()(int x, int y, int z) const
{
	return (size_t)x | ((size_t)y << 21) | ((size_t)z << 42);
}

size_t ChunkHash::operator()(const Int3& pos) const
{
	return (size_t)pos.x | ((size_t)pos.y << 21) | ((size_t)pos.z << 42);
}

//============================================================================