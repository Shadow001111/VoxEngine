#include "Chunk.h"

#include "Vec2.h"

#include <assert.h>
#include <vector>

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
	vao(0), vbo(0), instanceVBO(0), instanceCount(0)
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
void Chunk::init(int x, int y, int z)
{
	// Set position
	position = Int3(x, y, z);

	// Clear blocks
	for (int i = 0; i < CHUNK_VOLUME; i++)
	{
		blocks[i] = Block::Air;
	}

	// Create buffers (Assert if buffers already exist)
	assert(vao == 0 && vbo == 0 && instanceVBO == 0);
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &instanceVBO);
	instanceCount = 0;
}

// Cleans up resources
void Chunk::destroy()
{
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
	instanceCount = 0;
}

// Fills 'blocks' array
void Chunk::buildBlocks()
{
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
				if (getBlock_checkNeighbours(x - 1, y, z) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 0);
				}
				// +X
				if (getBlock_checkNeighbours(x + 1, y, z) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 1);
				}
				// -Y
				if (getBlock_checkNeighbours(x, y - 1, z) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 2);
				}
				// +Y
				if (getBlock_checkNeighbours(x, y + 1, z) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 3);
				}
				// -Z
				if (getBlock_checkNeighbours(x, y, z - 1) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 4);
				}
				// +Z
				if (getBlock_checkNeighbours(x, y, z + 1) == Block::Air)
				{
					mesh.emplace_back(x, y, z, 5);
				}
			}
		}
	}

	// Upload to GPU
	{
		Vec2 vertices[4] = // CCW order
		{
			{ 0.0f, 0.0f },
			{ 1.0f, 0.0f },
			{ 1.0f, 1.0f },
			{ 0.0f, 1.0f }
		};

		// Generate new buffers
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &instanceVBO);

		glBindVertexArray(vao);

		// Vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), (void*)0);

		// Instance buffer
		glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
		glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(BlockFaceInstance), mesh.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribIPointer(1, 1, GL_INT, sizeof(BlockFaceInstance), (void*)0); // integer attribute
		glVertexAttribDivisor(1, 1); // advance per instance

		glBindVertexArray(0);

		// Store instance count
		instanceCount = mesh.size();
	}
}

void Chunk::render() const
{
	if (instanceCount == 0) return;
	glBindVertexArray(vao);
	glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, instanceCount);
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
Block Chunk::getBlock_checkNeighbours(int x, int y, int z) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx != 0 || ny != 0 || nz != 0)
	{
		return Block::Air;
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