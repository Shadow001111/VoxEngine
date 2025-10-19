#pragma once
#include "Graphics/OpenGL_Buffer.h"

#include "MeshData.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>

class ChunkMeshManager
{
	GLuint vao, vbo;
	OpenGL_Buffer instanceVBO;

	BlockAllocator chunkBlockAllocator;

	// Exists only for REORGANIZATION
	std::unordered_map<size_t, MeshData*> allocatedMeshes;

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