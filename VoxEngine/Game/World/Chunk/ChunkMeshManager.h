#pragma once
#include "Graphics/OpenGL_Buffer.h"

#include "MeshData.h"

#include <vector>

class ChunkMeshManager
{
	GLuint vao, vbo;
	OpenGL_Buffer instanceVBO;

	BlockAllocator chunkBlockAllocator;

	ChunkMeshManager();
	~ChunkMeshManager();

	void configureInstanceVBO();
public:
	static ChunkMeshManager& getInstance();

	void preallocateMemory(size_t instanceCount);

	void processMeshRequests(std::vector<MeshData*>& meshRequests);

	OpenGL_Buffer& getInstanceVBO();

	void bindVAO() const;

	// Debug
	size_t getGaps() const;
};