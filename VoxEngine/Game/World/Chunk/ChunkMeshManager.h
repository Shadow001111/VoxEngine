#pragma once
#include "Graphics/OpenGL_Buffer.h"

#include "MeshData.h"

#include <vector>
#include <functional>

class ChunkMeshManager
{
	struct ProcessorConfig
	{
		std::function<void()> configureVBO;
		std::function<size_t(ChunkMeshData*)> getFaceCount;
		std::function<bool(ChunkMeshData*)> isCreated;
		std::function<BlockAllocator::Block& (ChunkMeshData*)> getAllocatedBlock;
		std::function<void(ChunkMeshData*, bool)> setCreated;
		std::function<void(ChunkMeshData*, BlockAllocator::Block)> setAllocatedBlock;
		size_t faceSize;
		std::string debugName;
	};

	struct MeshAllocator
	{
		GLuint vaoID;
		OpenGL_Buffer instanceVBO;
		BlockAllocator blockAllocator;
		ProcessorConfig config;

		MeshAllocator();
		~MeshAllocator();

		void init(const OpenGL_Buffer& quadVBO, const ProcessorConfig& config);

		void processMeshRequests(std::vector<ChunkMeshData*>& meshRequests);
	};
	
	OpenGL_Buffer vbo;

	MeshAllocator alignedMeshAllocator;
	MeshAllocator nonAlignedMeshAllocator;

	ChunkMeshManager();
	~ChunkMeshManager();

	void configureAlignedInstanceVBO();
	void configureNonAlignedInstanceVBO();
public:
	static ChunkMeshManager& getInstance();

	void processMeshRequests(std::vector<ChunkMeshData*>& alignedMeshRequests, std::vector<ChunkMeshData*>& nonAlignedMeshRequests);
public:
	OpenGL_Buffer& getAlignedInstanceVBO() { return alignedMeshAllocator.instanceVBO; };
	OpenGL_Buffer& getNonAlignedInstanceVBO() { return nonAlignedMeshAllocator.instanceVBO; };

	void bindAlignedVAO() const;
	void bindNonAlignedVAO() const;
};