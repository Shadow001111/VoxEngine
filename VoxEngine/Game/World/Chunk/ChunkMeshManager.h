#pragma once
#include "OpenGLWrappers/ImmutableBuffer.h"
#include "OpenGLWrappers/VertexArray.h"

#include "MeshData.h"

#include <vector>
#include <functional>

class ChunkMeshManager
{
	constexpr static GLbitfield INSTANCE_VBO_FLAGS = GL_DYNAMIC_STORAGE_BIT;

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
		VertexArray vao;
		ImmutableBuffer instanceVBO;
		BlockAllocator blockAllocator;
		ProcessorConfig config;

		MeshAllocator();
		~MeshAllocator();

		void init(const ProcessorConfig& config);

		void processMeshRequests(std::vector<ChunkMeshData*>& meshRequests);
	};
	
	ImmutableBuffer vbo;

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
	ImmutableBuffer& getAlignedInstanceVBO() { return alignedMeshAllocator.instanceVBO; };
	ImmutableBuffer& getNonAlignedInstanceVBO() { return nonAlignedMeshAllocator.instanceVBO; };

	void bindAlignedVAO() const { alignedMeshAllocator.vao.bind(); };
	void bindNonAlignedVAO() const { nonAlignedMeshAllocator.vao.bind(); };
};