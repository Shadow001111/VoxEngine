#pragma once
#include "OpenGLWrappers/ImmutableBuffer.h"
#include "OpenGLWrappers/VertexArray.h"

#include "ChunkMeshFaceStorage.h"

#include "Core/Container/DynamicArray.h"

#include <functional>

class ChunkMeshAllocator
{
	constexpr static GLbitfield INSTANCE_VBO_FLAGS = GL_DYNAMIC_STORAGE_BIT;

	struct ProcessorConfig
	{
		std::function<void()> configureVBO;
		std::function<uint32_t(ChunkMeshFaceStorage*)> getFaceCount;
		std::function<bool(ChunkMeshFaceStorage*)> isCreated;
		std::function<BlockAllocator<uint32_t>::Block& (ChunkMeshFaceStorage*)> getAllocatedBlock;
		std::function<void(ChunkMeshFaceStorage*, bool)> setCreated;
		std::function<void(ChunkMeshFaceStorage*, BlockAllocator<uint32_t>::Block)> setAllocatedBlock;
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

		void processMeshRequests(const DynamicArray<ChunkMeshFaceStorage*>& meshRequests);
	};
	
	ImmutableBuffer vbo;

	MeshAllocator alignedMeshAllocator;
	MeshAllocator nonAlignedMeshAllocator;

	ChunkMeshAllocator();
	~ChunkMeshAllocator() = default;

	void configureAlignedInstanceVBO();
	void configureNonAlignedInstanceVBO();
public:
	static ChunkMeshAllocator& getInstance();

	void processMeshAllocationRequests(
		const DynamicArray<ChunkMeshFaceStorage*>& alignedMeshRequests,
		const DynamicArray<ChunkMeshFaceStorage*>& nonAlignedMeshRequests
	);
public:
	ImmutableBuffer& getAlignedInstanceVBO() { return alignedMeshAllocator.instanceVBO; };
	ImmutableBuffer& getNonAlignedInstanceVBO() { return nonAlignedMeshAllocator.instanceVBO; };

	void bindAlignedVAO() const { alignedMeshAllocator.vao.bind(); };
	void bindNonAlignedVAO() const { nonAlignedMeshAllocator.vao.bind(); };
};