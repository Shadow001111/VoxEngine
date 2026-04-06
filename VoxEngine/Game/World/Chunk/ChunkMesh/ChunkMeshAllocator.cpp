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
			[this]() { configureAlignedInstanceVBO(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->getAlignedFaceCount(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->alignedCreated; },
			[](ChunkMeshFaceStorage* mesh) -> BlockAllocator<uint32_t>::Block& { return mesh->allocatedBlock_alignedFaces; },
			[](ChunkMeshFaceStorage* mesh, bool created) { mesh->alignedCreated = created; },
			[](ChunkMeshFaceStorage* mesh, BlockAllocator<uint32_t>::Block block) { mesh->allocatedBlock_alignedFaces = block; },
			sizeof(AlignedBlockFace),
			"processAlignedMeshRequests"
		};
		alignedMeshAllocator.init(config);
	}
	{
		ProcessorConfig config = {
			[this]() { configureUnalignedInstanceVBO(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->getUnalignedFaceCount(); },
			[](ChunkMeshFaceStorage* mesh) { return mesh->unalignedCreated; },
			[](ChunkMeshFaceStorage* mesh) -> BlockAllocator<uint32_t>::Block& { return mesh->allocatedBlock_unalignedFaces; },
			[](ChunkMeshFaceStorage* mesh, bool created) { mesh->unalignedCreated = created; },
			[](ChunkMeshFaceStorage* mesh, BlockAllocator<uint32_t>::Block block) { mesh->allocatedBlock_unalignedFaces = block; },
			sizeof(UnalignedBlockFace),
			"processUnalignedMeshRequests"
		};
		unalignedMeshAllocator.init(config);
	}

	{
		alignedMeshAllocator.vao.bindVertexBuffer(0, vbo.getID(), 0, 2 * sizeof(float));

		alignedMeshAllocator.vao.enableAttribute(0);
		alignedMeshAllocator.vao.setFloatAttribute(0, 2, 0, 0);

		configureAlignedInstanceVBO();
	}

	{
		unalignedMeshAllocator.vao.bindVertexBuffer(0, vbo.getID(), 0, 2 * sizeof(float));

		unalignedMeshAllocator.vao.enableAttribute(0);
		unalignedMeshAllocator.vao.setFloatAttribute(0, 2, 0, 0);

		configureUnalignedInstanceVBO();
	}
}

void ChunkMeshAllocator::configureAlignedInstanceVBO()
{
	auto& instanceVBO = alignedMeshAllocator.instanceVBO;

	// Vertex attributes
	alignedMeshAllocator.vao.bindVertexBuffer(1, instanceVBO.getID(), 0, sizeof(AlignedBlockFace));

	alignedMeshAllocator.vao.enableAttribute(1);
	alignedMeshAllocator.vao.setIntAttribute(1, 2, 0, 1, GL_UNSIGNED_INT);
	alignedMeshAllocator.vao.setAttributeDivisor(1, 1);
}

void ChunkMeshAllocator::configureUnalignedInstanceVBO()
{
	auto& instanceVBO = unalignedMeshAllocator.instanceVBO;

	// Vertex attributes
	unalignedMeshAllocator.vao.bindVertexBuffer(1, instanceVBO.getID(), 0, sizeof(UnalignedBlockFace));

	// Block position + Us
	unalignedMeshAllocator.vao.enableAttribute(1);
	unalignedMeshAllocator.vao.setIntAttribute(1, 1, 0, 1, GL_UNSIGNED_INT);
	unalignedMeshAllocator.vao.setAttributeDivisor(1, 1);

	// Vertex shifts
	unalignedMeshAllocator.vao.enableAttribute(2);
	unalignedMeshAllocator.vao.setIntAttribute(2, 2, 1 * sizeof(int), 1, GL_UNSIGNED_INT);
	unalignedMeshAllocator.vao.setAttributeDivisor(2, 1);

	// Vs + textureID
	unalignedMeshAllocator.vao.enableAttribute(3);
	unalignedMeshAllocator.vao.setIntAttribute(3, 1, 3 * sizeof(int), 1, GL_UNSIGNED_INT);
	unalignedMeshAllocator.vao.setAttributeDivisor(3, 1);

	// Light
	unalignedMeshAllocator.vao.enableAttribute(4);
	unalignedMeshAllocator.vao.setIntAttribute(4, 2, 4 * sizeof(int), 1, GL_UNSIGNED_INT);
	unalignedMeshAllocator.vao.setAttributeDivisor(4, 1);

	// AO
	unalignedMeshAllocator.vao.enableAttribute(5);
	unalignedMeshAllocator.vao.setIntAttribute(5, 1, 6 * sizeof(int), 1, GL_UNSIGNED_INT);
	unalignedMeshAllocator.vao.setAttributeDivisor(5, 1);
}

ChunkMeshAllocator& ChunkMeshAllocator::getInstance()
{
	static ChunkMeshAllocator instance;
	return instance;
}

void ChunkMeshAllocator::processMeshAllocationRequests(
	const DynamicArray<ChunkMeshFaceStorage*>& alignedMeshRequests,
	const DynamicArray<ChunkMeshFaceStorage*>& unalignedMeshRequests
)
{
	alignedMeshAllocator.processMeshRequests(alignedMeshRequests);
	unalignedMeshAllocator.processMeshRequests(unalignedMeshRequests);
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
