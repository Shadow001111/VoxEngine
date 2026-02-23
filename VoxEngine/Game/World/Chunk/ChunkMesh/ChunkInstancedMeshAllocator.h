#pragma once
#include "OpenGLWrappers/ImmutableBuffer.h"
#include "OpenGLWrappers/VertexArray.h"

#include "ChunkInstancedMeshFaceStorage.h"

#include <vector>
#include <functional>

class ChunkInstancedMeshAllocator
{
	constexpr static GLbitfield INSTANCE_VBO_FLAGS = GL_DYNAMIC_STORAGE_BIT;

	struct ProcessorConfig
	{
		std::function<void()> configureVBO;
		std::function<size_t(ChunkInstancedMeshFaceStorage*)> getFaceCount;
		std::function<bool(ChunkInstancedMeshFaceStorage*)> isCreated;
		std::function<BlockAllocator::Block& (ChunkInstancedMeshFaceStorage*)> getAllocatedBlock;
		std::function<void(ChunkInstancedMeshFaceStorage*, bool)> setCreated;
		std::function<void(ChunkInstancedMeshFaceStorage*, BlockAllocator::Block)> setAllocatedBlock;
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
		~MeshAllocator() = default;

		void init(const ProcessorConfig& config);

		void processMeshRequests(const std::vector<ChunkInstancedMeshFaceStorage*>& meshRequests);
	};
	
	ImmutableBuffer vbo;

	MeshAllocator alignedMeshAllocator;
	MeshAllocator nonAlignedMeshAllocator;

	ChunkInstancedMeshAllocator();
	~ChunkInstancedMeshAllocator() = default;

	void configureAlignedInstanceVBO();
	void configureNonAlignedInstanceVBO();
public:
	static ChunkInstancedMeshAllocator& getInstance();

	void processMeshAllocationRequests(
		const std::vector<ChunkInstancedMeshFaceStorage*>& alignedMeshRequests,
		const std::vector<ChunkInstancedMeshFaceStorage*>& nonAlignedMeshRequests
	);
public:
	ImmutableBuffer& getAlignedInstanceVBO() { return alignedMeshAllocator.instanceVBO; };
	ImmutableBuffer& getNonAlignedInstanceVBO() { return nonAlignedMeshAllocator.instanceVBO; };

	void bindAlignedVAO() const { alignedMeshAllocator.vao.bind(); };
	void bindNonAlignedVAO() const { nonAlignedMeshAllocator.vao.bind(); };
};