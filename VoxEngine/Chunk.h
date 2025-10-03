#pragma once
#include "Block.h"
#include "Metrics.h"

#include "Int3.h"

#include <glad/glad.h>

#include <memory>

// TODO: Maybe 'blocks' should be a pointer to a dynamically allocated array, so it can be moved without copying?
// TODO: Implement chunk pool
class Chunk
{
	Int3 position; // Chunk coordinates in chunk space
	Block blocks[CHUNK_VOLUME];

	GLuint vao, vbo, instanceVBO; // Buffers
	size_t instanceCount;

	static size_t getIndex(int x, int y, int z);
public:
	Chunk();
	~Chunk();

	bool operator==(const Chunk& other) const;

	void init(int x, int y, int z);
	void destroy();

	void buildBlocks();
	void buildMesh();

	void render() const;

	Block getBlock_inBoundaries(int x, int y, int z) const;
	Block getBlock_checkNeighbours(int x, int y, int z) const;

	int getX() const;
	int getY() const;
	int getZ() const;
	Int3 getPosition() const;
};

class ChunkHash
{
public:
	size_t operator()(const Chunk& chunk) const;
	size_t operator()(const Chunk* chunk) const;
	size_t operator()(const std::unique_ptr<Chunk>& chunk) const;
	size_t operator()(int x, int y, int z) const;
	size_t operator()(const Int3& pos) const;
};