#pragma once
#include "Block.h"
#include "Metrics.h"

#include <glad/glad.h>

// TODO: Maybe 'blocks' should be a pointer to a dynamically allocated array, so it can be moved without copying?
// TODO: Implement chunk pool
class Chunk
{
	int X, Y, Z; // Chunk coordinates in chunk space
	Block blocks[CHUNK_VOLUME];

	GLuint vao, vbo, instanceVBO; // Buffers
	size_t instanceCount;

	static size_t getIndex(int x, int y, int z);
public:
	Chunk();
	~Chunk();

	void init(int x, int y, int z);
	void destroy();

	void buildBlocks();
	void buildMesh();

	void render() const;

	Block getBlock_inBoundaries(int x, int y, int z) const;
	Block getBlock_checkNeighbours(int x, int y, int z) const;
};