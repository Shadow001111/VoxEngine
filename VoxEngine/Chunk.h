#pragma once
#include "Block.h"
#include "Metrics.h"

#include "Int3.h"

#include <glad/glad.h>

// TODO: Maybe 'blocks' should be a pointer to a dynamically allocated array, so it can be moved without copying?
class Chunk
{
	Int3 position; // Chunk coordinates in chunk space
	Block blocks[CHUNK_VOLUME];

	GLuint vao, vbo, instanceVBO; // Buffers
	size_t faceCount;
	size_t faceCapacity;

	bool loadedChunkColumnData;

	static size_t getIndex(int x, int y, int z);
public:
	Chunk* neighbors[6]; // Pointers to neighboring chunks, for easier access when building mesh

	Chunk();
	~Chunk();

	Chunk(const Chunk&) = delete;
	Chunk& operator=(const Chunk&) = delete;
	Chunk(Chunk&&) = delete;
	Chunk& operator=(Chunk&&) = delete;

	bool operator==(const Chunk& other) const;

	void init(int x, int y, int z, Chunk** neighbors);
	void destroy();

	void buildBlocks();
	void buildMesh();

	void render() const;

	Block getBlock_inBoundaries(int x, int y, int z) const;
	Block getBlock_checkNeighbors(int x, int y, int z) const;

	int getX() const;
	int getY() const;
	int getZ() const;
	Int3 getPosition() const;

	// Debug
	size_t getFaceCount() const;
	size_t getFaceCapacity() const;
};