#pragma once
#include "OpenGLWrappers/ImmutableBuffer.h"
#include "OpenGLWrappers/VertexArray.h"

#include "ChunkMeshFaceStorage.h"

#include "Core/Container/DynamicArray.h"

#include <functional>

// TODO: Make this whole class template based
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

	MeshAllocator alignedOpaqueMeshAllocator;
	MeshAllocator alignedTranslucentMeshAllocator;
	MeshAllocator unalignedOpaqueMeshAllocator;
	MeshAllocator unalignedTranslucentMeshAllocator;

	ChunkMeshAllocator();
	~ChunkMeshAllocator() = default;

	void configureAlignedOpaqueInstanceVBO();
	void configureAlignedTranslucentInstanceVBO();
	void configureUnalignedOpaqueInstanceVBO();
	void configureUnalignedTranslucentInstanceVBO();
public:
	static ChunkMeshAllocator& getInstance();

	void processMeshAllocationRequests(
		const DynamicArray<ChunkMeshFaceStorage*>& alignedOpaqueMeshRequests,
		const DynamicArray<ChunkMeshFaceStorage*>& alignedTranslucentMeshRequests,
		const DynamicArray<ChunkMeshFaceStorage*>& unalignedOpaqueMeshRequests,
		const DynamicArray<ChunkMeshFaceStorage*>& unalignedTranslucentMeshRequests
	);
public:
	ImmutableBuffer& getAlignedOpaqueInstanceVBO() { return alignedOpaqueMeshAllocator.instanceVBO; };
	ImmutableBuffer& getAlignedTranslucentInstanceVBO() { return alignedTranslucentMeshAllocator.instanceVBO; };
	ImmutableBuffer& getUnalignedOpaqueInstanceVBO() { return unalignedOpaqueMeshAllocator.instanceVBO; };
	ImmutableBuffer& getUnalignedTranslucentInstanceVBO() { return unalignedTranslucentMeshAllocator.instanceVBO; };

	const ImmutableBuffer& getAlignedOpaqueInstanceVBO() const { return alignedOpaqueMeshAllocator.instanceVBO; };
	const ImmutableBuffer& getAlignedTranslucentInstanceVBO() const { return alignedTranslucentMeshAllocator.instanceVBO; };
	const ImmutableBuffer& getUnalignedOpaqueInstanceVBO() const { return unalignedOpaqueMeshAllocator.instanceVBO; };
	const ImmutableBuffer& getUnalignedTranslucentInstanceVBO() const { return unalignedTranslucentMeshAllocator.instanceVBO; };

	void bindAlignedOpaqueVAO() const { alignedOpaqueMeshAllocator.vao.bind(); };
	void bindAlignedTranslucentVAO() const { alignedTranslucentMeshAllocator.vao.bind(); };
	void bindUnalignedOpaqueVAO() const { unalignedOpaqueMeshAllocator.vao.bind(); };
	void bindUnalignedTranslucentVAO() const { unalignedTranslucentMeshAllocator.vao.bind(); };
};