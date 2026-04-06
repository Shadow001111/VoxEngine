#include "ChunkMeshAllocator.h"

#include <iostream>

#include "Core/Assert.h"

ChunkMeshAllocator::ChunkMeshAllocator()
{
	// VBO
	const float vertices[8] = // CCW order
	{
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	vbo.create(GL_ARRAY_BUFFER);
	vbo.allocateStorage(sizeof(vertices), 0, vertices);

	{
		ProcessorConfig config = {
			[this]() { configureAlignedOpaqueInstanceVBO(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->getAlignedOpaqueFaceCount(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->readFlag(ChunkMeshFaceStorage::Flag::AlignedOpaqueCreated); },
			[](ChunkMeshFaceStorage* mesh) -> BlockAllocator<uint32_t>::Block& { return mesh->alignedOpaqueFacesBlock; },
			[](ChunkMeshFaceStorage* mesh, bool created) { mesh->setFlag(ChunkMeshFaceStorage::Flag::AlignedOpaqueCreated, created); },
			[](ChunkMeshFaceStorage* mesh, BlockAllocator<uint32_t>::Block block) { mesh->alignedOpaqueFacesBlock = block; },
			sizeof(AlignedBlockFace),
			"processAlignedOpaqueMeshRequests"
		};
		alignedOpaqueMeshAllocator.init(config);
	}
	{
		ProcessorConfig config = {
			[this]() { configureAlignedTranslucentInstanceVBO(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->getAlignedTranslucentFaceCount(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->readFlag(ChunkMeshFaceStorage::Flag::AlignedTranslucentCreated); },
			[](ChunkMeshFaceStorage* mesh) -> BlockAllocator<uint32_t>::Block& { return mesh->alignedTranslucentFacesBlock; },
			[](ChunkMeshFaceStorage* mesh, bool created) { mesh->setFlag(ChunkMeshFaceStorage::Flag::AlignedTranslucentCreated, created); },
			[](ChunkMeshFaceStorage* mesh, BlockAllocator<uint32_t>::Block block) { mesh->alignedTranslucentFacesBlock = block; },
			sizeof(AlignedBlockFace),
			"processAlignedTranslucentMeshRequests"
		};
		alignedTranslucentMeshAllocator.init(config);
	}
	{
		ProcessorConfig config = {
			[this]() { configureUnalignedOpaqueInstanceVBO(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->getUnalignedOpaqueFaceCount(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->readFlag(ChunkMeshFaceStorage::Flag::UnalignedOpaqueCreated); },
			[](ChunkMeshFaceStorage* mesh) -> BlockAllocator<uint32_t>::Block& { return mesh->unalignedOpaqueFacesBlock; },
			[](ChunkMeshFaceStorage* mesh, bool created) { mesh->setFlag(ChunkMeshFaceStorage::Flag::UnalignedOpaqueCreated, created); },
			[](ChunkMeshFaceStorage* mesh, BlockAllocator<uint32_t>::Block block) { mesh->unalignedOpaqueFacesBlock = block; },
			sizeof(UnalignedBlockFace),
			"processUnalignedOpaqueMeshRequests"
		};
		unalignedOpaqueMeshAllocator.init(config);
	}
	{
		ProcessorConfig config = {
			[this]() { configureUnalignedTranslucentInstanceVBO(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->getUnalignedTranslucentFaceCount(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->readFlag(ChunkMeshFaceStorage::Flag::UnalignedTranslucentCreated); },
			[](ChunkMeshFaceStorage* mesh) -> BlockAllocator<uint32_t>::Block& { return mesh->unalignedTranslucentFacesBlock; },
			[](ChunkMeshFaceStorage* mesh, bool created) { mesh->setFlag(ChunkMeshFaceStorage::Flag::UnalignedTranslucentCreated, created); },
			[](ChunkMeshFaceStorage* mesh, BlockAllocator<uint32_t>::Block block) { mesh->unalignedTranslucentFacesBlock = block; },
			sizeof(UnalignedBlockFace),
			"processUnalignedTranslucentMeshRequests"
		};
		unalignedTranslucentMeshAllocator.init(config);
	}

	{
		alignedOpaqueMeshAllocator.vao.bindVertexBuffer(0, vbo.getID(), 0, 2 * sizeof(float));

		alignedOpaqueMeshAllocator.vao.enableAttribute(0);
		alignedOpaqueMeshAllocator.vao.setFloatAttribute(0, 2, 0, 0);

		configureAlignedOpaqueInstanceVBO();
	}
	{
		alignedTranslucentMeshAllocator.vao.bindVertexBuffer(0, vbo.getID(), 0, 2 * sizeof(float));

		alignedTranslucentMeshAllocator.vao.enableAttribute(0);
		alignedTranslucentMeshAllocator.vao.setFloatAttribute(0, 2, 0, 0);

		configureAlignedTranslucentInstanceVBO();
	}
	{
		unalignedOpaqueMeshAllocator.vao.bindVertexBuffer(0, vbo.getID(), 0, 2 * sizeof(float));

		unalignedOpaqueMeshAllocator.vao.enableAttribute(0);
		unalignedOpaqueMeshAllocator.vao.setFloatAttribute(0, 2, 0, 0);

		configureUnalignedOpaqueInstanceVBO();
	}
	{
		unalignedTranslucentMeshAllocator.vao.bindVertexBuffer(0, vbo.getID(), 0, 2 * sizeof(float));

		unalignedTranslucentMeshAllocator.vao.enableAttribute(0);
		unalignedTranslucentMeshAllocator.vao.setFloatAttribute(0, 2, 0, 0);

		configureUnalignedTranslucentInstanceVBO();
	}
}

void ChunkMeshAllocator::configureAlignedOpaqueInstanceVBO()
{
	auto& meshAllocator = alignedOpaqueMeshAllocator;
	auto& instanceVBO = meshAllocator.instanceVBO;

	// Vertex attributes
	meshAllocator.vao.bindVertexBuffer(1, instanceVBO.getID(), 0, sizeof(AlignedBlockFace));

	meshAllocator.vao.enableAttribute(1);
	meshAllocator.vao.setIntAttribute(1, 2, 0, 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(1, 1);
}

void ChunkMeshAllocator::configureAlignedTranslucentInstanceVBO()
{
	auto& meshAllocator = alignedTranslucentMeshAllocator;
	auto& instanceVBO = meshAllocator.instanceVBO;

	// Vertex attributes
	meshAllocator.vao.bindVertexBuffer(1, instanceVBO.getID(), 0, sizeof(AlignedBlockFace));

	meshAllocator.vao.enableAttribute(1);
	meshAllocator.vao.setIntAttribute(1, 2, 0, 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(1, 1);
}

void ChunkMeshAllocator::configureUnalignedOpaqueInstanceVBO()
{
	auto& meshAllocator = unalignedOpaqueMeshAllocator;
	auto& instanceVBO = meshAllocator.instanceVBO;

	// Vertex attributes
	meshAllocator.vao.bindVertexBuffer(1, instanceVBO.getID(), 0, sizeof(UnalignedBlockFace));

	// Block position + Us
	meshAllocator.vao.enableAttribute(1);
	meshAllocator.vao.setIntAttribute(1, 1, 0, 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(1, 1);

	// Vertex shifts
	meshAllocator.vao.enableAttribute(2);
	meshAllocator.vao.setIntAttribute(2, 2, 1 * sizeof(int), 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(2, 1);

	// Vs + textureID
	meshAllocator.vao.enableAttribute(3);
	meshAllocator.vao.setIntAttribute(3, 1, 3 * sizeof(int), 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(3, 1);

	// Light
	meshAllocator.vao.enableAttribute(4);
	meshAllocator.vao.setIntAttribute(4, 2, 4 * sizeof(int), 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(4, 1);

	// AO
	meshAllocator.vao.enableAttribute(5);
	meshAllocator.vao.setIntAttribute(5, 1, 6 * sizeof(int), 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(5, 1);
}

void ChunkMeshAllocator::configureUnalignedTranslucentInstanceVBO()
{
	auto& meshAllocator = unalignedTranslucentMeshAllocator;
	auto& instanceVBO = meshAllocator.instanceVBO;

	// Vertex attributes
	meshAllocator.vao.bindVertexBuffer(1, instanceVBO.getID(), 0, sizeof(UnalignedBlockFace));

	// Block position + Us
	meshAllocator.vao.enableAttribute(1);
	meshAllocator.vao.setIntAttribute(1, 1, 0, 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(1, 1);

	// Vertex shifts
	meshAllocator.vao.enableAttribute(2);
	meshAllocator.vao.setIntAttribute(2, 2, 1 * sizeof(int), 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(2, 1);

	// Vs + textureID
	meshAllocator.vao.enableAttribute(3);
	meshAllocator.vao.setIntAttribute(3, 1, 3 * sizeof(int), 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(3, 1);

	// Light
	meshAllocator.vao.enableAttribute(4);
	meshAllocator.vao.setIntAttribute(4, 2, 4 * sizeof(int), 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(4, 1);

	// AO
	meshAllocator.vao.enableAttribute(5);
	meshAllocator.vao.setIntAttribute(5, 1, 6 * sizeof(int), 1, GL_UNSIGNED_INT);
	meshAllocator.vao.setAttributeDivisor(5, 1);
}

ChunkMeshAllocator& ChunkMeshAllocator::getInstance()
{
	static ChunkMeshAllocator instance;
	return instance;
}

void ChunkMeshAllocator::processMeshAllocationRequests(
	const DynamicArray<ChunkMeshFaceStorage*>& alignedOpaqueMeshRequests,
	const DynamicArray<ChunkMeshFaceStorage*>& alignedTranslucentMeshRequests,
	const DynamicArray<ChunkMeshFaceStorage*>& unalignedOpaqueMeshRequests,
	const DynamicArray<ChunkMeshFaceStorage*>& unalignedTranslucentMeshRequests
)
{
	alignedOpaqueMeshAllocator.processMeshRequests(alignedOpaqueMeshRequests);
	alignedTranslucentMeshAllocator.processMeshRequests(alignedTranslucentMeshRequests);
	unalignedOpaqueMeshAllocator.processMeshRequests(unalignedOpaqueMeshRequests);
	unalignedTranslucentMeshAllocator.processMeshRequests(unalignedTranslucentMeshRequests);
}

ChunkMeshAllocator::MeshAllocator::MeshAllocator() :
	blockAllocator(0)
{
	vao.create();
	instanceVBO.create(GL_ARRAY_BUFFER);
}

void ChunkMeshAllocator::MeshAllocator::init(const ProcessorConfig& config)
{
	this->config = config;
}

void ChunkMeshAllocator::MeshAllocator::processMeshRequests(const DynamicArray<ChunkMeshFaceStorage*>& meshRequests)
{
	// Store old capacity
	size_t oldCapacity = blockAllocator.getCapacity();

	// Free blocks
	for (ChunkMeshFaceStorage* chunkMesh : meshRequests)
	{
		if (!config.isCreated(chunkMesh))
		{
			continue;
		}

		ASSERT(blockAllocator.free(config.getAllocatedBlock(chunkMesh)));
	}

	// Allocate blocks
	size_t stopIndex = 0;
	for (ChunkMeshFaceStorage* chunkMesh : meshRequests)
	{
		auto faceCount = config.getFaceCount(chunkMesh);

		auto result = blockAllocator.allocate(faceCount);
		if (!result.has_value())
		{
			break;
		}

		config.setCreated(chunkMesh, true);
		config.setAllocatedBlock(chunkMesh, result.value());
		stopIndex++;
	}

	if (stopIndex == meshRequests.size())
	{
		return;
	}

	// Allocate more memory
	size_t newCapacity = blockAllocator.getLastBlockEnd();
	for (size_t i = stopIndex; i < meshRequests.size(); i++)
	{
		newCapacity += config.getFaceCount(meshRequests[i]);
	}
	newCapacity += (newCapacity >> 1); // *= 1.5

	blockAllocator.setCapacity(newCapacity);
	if (oldCapacity == 0)
	{
		instanceVBO.allocateStorage(newCapacity * config.faceSize, INSTANCE_VBO_FLAGS);
	}
	else
	{
		// Create new buffer
		ImmutableBuffer newBuffer;
		newBuffer.create(GL_ARRAY_BUFFER);
		newBuffer.allocateStorage(newCapacity * config.faceSize, INSTANCE_VBO_FLAGS);

		// Copy data to a new buffer
		const DynamicArray<BlockAllocator<uint32_t>::Block>& currentBlocks = blockAllocator.getAllAllocations();
		const auto& firstBlock = currentBlocks[0];
		const auto& lastBlock = currentBlocks[currentBlocks.size() - 1];
		newBuffer.copyRangeFrom(
			instanceVBO,
			firstBlock.offset * config.faceSize,
			firstBlock.offset * config.faceSize,
			(lastBlock.offset + lastBlock.size - firstBlock.offset) * config.faceSize
		);

		// Replace old with new buffer
		instanceVBO = std::move(newBuffer);
		config.configureVBO();
	}

	// Allocate the rest of blocks
	for (size_t i = stopIndex; i < meshRequests.size(); i++)
	{
		ChunkMeshFaceStorage* chunkMesh = meshRequests[i];
		auto faceCount = config.getFaceCount(chunkMesh);

		auto result = blockAllocator.allocate(faceCount);
		if (!result.has_value())
		{
			std::cerr << "[ChunkMeshAllocator]: " << config.debugName << ": mesh wasn't created.\n";
			break;
		}

		config.setCreated(chunkMesh, true);
		config.setAllocatedBlock(chunkMesh, result.value());
	}
}
