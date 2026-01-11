#pragma once
#include "OpenGLWrappers/OpenGL_ImmutableBuffer.h"
#include "OpenGLWrappers/OpenGL_VAO.h"

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
		OpenGL_VAO vao;
		OpenGL_ImmutableBuffer instanceVBO;
		BlockAllocator blockAllocator;
		ProcessorConfig config;

		MeshAllocator();
		~MeshAllocator();

		void init(const ProcessorConfig& config);

		void processMeshRequests(std::vector<ChunkMeshData*>& meshRequests);
	};
	
	OpenGL_ImmutableBuffer vbo;

	MeshAllocator alignedMeshAllocator;
	MeshAllocator nonAlignedMeshAllocator;

	ChunkMeshManager();
	~ChunkMeshManager() = default;

	void configureAlignedInstanceVBO();
	void configureNonAlignedInstanceVBO();
public:
	static ChunkMeshManager& getInstance();

	void processMeshRequests(std::vector<ChunkMeshData*>& alignedMeshRequests, std::vector<ChunkMeshData*>& nonAlignedMeshRequests);
public:
	OpenGL_ImmutableBuffer& getAlignedInstanceVBO() { return alignedMeshAllocator.instanceVBO; };
	OpenGL_ImmutableBuffer& getNonAlignedInstanceVBO() { return nonAlignedMeshAllocator.instanceVBO; };

	void bindAlignedVAO() const { alignedMeshAllocator.vao.bind(); };
	void bindNonAlignedVAO() const { nonAlignedMeshAllocator.vao.bind(); };
};