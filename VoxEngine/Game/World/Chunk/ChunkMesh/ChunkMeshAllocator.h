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
		std::function<ChunkMeshFaceStorage::Block& (ChunkMeshFaceStorage*)> getAllocatedBlock;
		std::function<void(ChunkMeshFaceStorage*, bool)> setCreated;
		std::function<void(ChunkMeshFaceStorage*, ChunkMeshFaceStorage::Block)> setAllocatedBlock;
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
	
	ImmutableBuffer vbo; // Shared quad geometry

	std::array<MeshAllocator, (size_t)MeshLayer::Count> meshAllocators;

	ChunkMeshAllocator();
	~ChunkMeshAllocator() = default;

	void configureInstanceVBO(MeshLayer layer);
public:
	static ChunkMeshAllocator& getInstance();

	using LayerRequests = std::array<DynamicArray<ChunkMeshFaceStorage*>, 4>;

	void processMeshAllocationRequests(const LayerRequests& requests);
public:
	ImmutableBuffer& getInstanceVBO(MeshLayer layer) { return meshAllocators[static_cast<size_t>(layer)].instanceVBO; };
	const ImmutableBuffer& getInstanceVBO(MeshLayer layer) const { return meshAllocators[static_cast<size_t>(layer)].instanceVBO; };

	void bindVAO(MeshLayer layer) const { meshAllocators[static_cast<size_t>(layer)].vao.bind(); }
};