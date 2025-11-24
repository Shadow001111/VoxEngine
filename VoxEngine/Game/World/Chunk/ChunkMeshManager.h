#pragma once
#include "Graphics/OpenGL_Buffer.h"

#include "MeshData.h"

#include <vector>

class ChunkMeshManager
{
	GLuint alignedVAO, nonAlignedVAO;
	OpenGL_Buffer vbo;

	OpenGL_Buffer alignedInstanceVBO;
	OpenGL_Buffer nonAlignedInstanceVBO;

	BlockAllocator alignedBlockAllocator;
	BlockAllocator nonAlignedBlockAllocator;

	ChunkMeshManager();
	~ChunkMeshManager();

	void configureAlignedInstanceVBO();
	void configureNonAlignedInstanceVBO();
public:
	static ChunkMeshManager& getInstance();

	void processMeshRequests(std::vector<ChunkMeshData*>& meshRequests);
private:
	void processAlignedMeshRequests(std::vector<ChunkMeshData*>& meshRequests);
	void processNonAlignedMeshRequests(std::vector<ChunkMeshData*>& meshRequests);
public:
	OpenGL_Buffer& getAlignedInstanceVBO() { return alignedInstanceVBO; };
	OpenGL_Buffer& getNonAlignedInstanceVBO() { return nonAlignedInstanceVBO; };

	void bindAlignedVAO() const;
	void bindNonAlignedVAO() const;
};