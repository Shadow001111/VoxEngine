#pragma once
#include "OpenGLWrappers/ImmutableBuffer.h"
#include "OpenGLWrappers/VertexArray.h"

#include "ChunkInstancedMeshFaceStorage.h"

#include "Core/Container/DynamicArray.h"

#include <functional>

class ChunkInstancedMeshAllocator
{
	constexpr static GLbitfield INSTANCE_VBO_FLAGS = GL_DYNAMIC_STORAGE_BIT;

	struct ProcessorConfig
	{
		std::function<void()> configureVBO;
		std::function<uint32_t(ChunkInstancedMeshFaceStorage*)> getFaceCount;
		std::function<bool(ChunkInstancedMeshFaceStorage*)> isCreated;
		std::function<BlockAllocator<uint32_t>::Block& (ChunkInstancedMeshFaceStorage*)> getAllocatedBlock;
		std::function<void(ChunkInstancedMeshFaceStorage*, bool)> setCreated;
		std::function<void(ChunkInstancedMeshFaceStorage*, BlockAllocator<uint32_t>::Block)> setAllocatedBlock;
		size_t faceSize;
		std::string debugName;
	};

	struct MeshAllocator
	{
		VertexArray vao;
		ImmutableBuffer instanceVBO;
		BlockAllocator<uint32_t> blockAllocator;
		ProcessorConfig config;

		MeshAllocator();
		~MeshAllocator() = default;

		void init(const ProcessorConfig& config);

		void processMeshRequests(const DynamicArray<ChunkInstancedMeshFaceStorage*>& meshRequests);
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
		const DynamicArray<ChunkInstancedMeshFaceStorage*>& alignedMeshRequests,
		const DynamicArray<ChunkInstancedMeshFaceStorage*>& nonAlignedMeshRequests
	);
public:
	ImmutableBuffer& getAlignedInstanceVBO() { return alignedMeshAllocator.instanceVBO; };
	ImmutableBuffer& getNonAlignedInstanceVBO() { return nonAlignedMeshAllocator.instanceVBO; };

	void bindAlignedVAO() const { alignedMeshAllocator.vao.bind(); };
	void bindNonAlignedVAO() const { nonAlignedMeshAllocator.vao.bind(); };
};