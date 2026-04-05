#include "ChunkInstancedMeshAllocator.h"

#include <iostream>

#include "Core/Assert.h"

ChunkInstancedMeshAllocator::ChunkInstancedMeshAllocator()
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
			[](ChunkInstancedMeshFaceStorage* mesh) { return mesh->getAlignedFaceCount(); },
			[](ChunkInstancedMeshFaceStorage* mesh) { return mesh->alignedCreated; },
			[](ChunkInstancedMeshFaceStorage* mesh) -> BlockAllocator<uint32_t>::Block& { return mesh->allocatedBlock_alignedFaces; },
			[](ChunkInstancedMeshFaceStorage* mesh, bool created) { mesh->alignedCreated = created; },
			[](ChunkInstancedMeshFaceStorage* mesh, BlockAllocator<uint32_t>::Block block) { mesh->allocatedBlock_alignedFaces = block; },
			sizeof(AlignedBlockFace),
			"processAlignedMeshRequests"
		};
		alignedMeshAllocator.init(config);
	}
	{
		ProcessorConfig config = {
			[this]() { configureNonAlignedInstanceVBO(); },
			[](ChunkInstancedMeshFaceStorage* mesh) { return mesh->getNonAlignedFaceCount(); },
			[](ChunkInstancedMeshFaceStorage* mesh) { return mesh->nonAlignedCreated; },
			[](ChunkInstancedMeshFaceStorage* mesh) -> BlockAllocator<uint32_t>::Block& { return mesh->allocatedBlock_nonAlignedFaces; },
			[](ChunkInstancedMeshFaceStorage* mesh, bool created) { mesh->nonAlignedCreated = created; },
			[](ChunkInstancedMeshFaceStorage* mesh, BlockAllocator<uint32_t>::Block block) { mesh->allocatedBlock_nonAlignedFaces = block; },
			sizeof(NonAlignedBlockFace),
			"processNonAlignedMeshRequests"
		};
		nonAlignedMeshAllocator.init(config);
	}

	{
		alignedMeshAllocator.vao.bindVertexBuffer(0, vbo.getID(), 0, 2 * sizeof(float));

		alignedMeshAllocator.vao.enableAttribute(0);
		alignedMeshAllocator.vao.setFloatAttribute(0, 2, 0, 0);

		configureAlignedInstanceVBO();
	}

	{
		nonAlignedMeshAllocator.vao.bindVertexBuffer(0, vbo.getID(), 0, 2 * sizeof(float));

		nonAlignedMeshAllocator.vao.enableAttribute(0);
		nonAlignedMeshAllocator.vao.setFloatAttribute(0, 2, 0, 0);

		configureNonAlignedInstanceVBO();
	}
}

void ChunkInstancedMeshAllocator::configureAlignedInstanceVBO()
{
	auto& instanceVBO = alignedMeshAllocator.instanceVBO;

	// Vertex attributes
	alignedMeshAllocator.vao.bindVertexBuffer(1, instanceVBO.getID(), 0, sizeof(AlignedBlockFace));

	alignedMeshAllocator.vao.enableAttribute(1);
	alignedMeshAllocator.vao.setIntAttribute(1, 2, 0, 1, GL_UNSIGNED_INT);
	alignedMeshAllocator.vao.setAttributeDivisor(1, 1);
}

void ChunkInstancedMeshAllocator::configureNonAlignedInstanceVBO()
{
	auto& instanceVBO = nonAlignedMeshAllocator.instanceVBO;

	// Vertex attributes
	nonAlignedMeshAllocator.vao.bindVertexBuffer(1, instanceVBO.getID(), 0, sizeof(NonAlignedBlockFace));

	// Block position + Us
	nonAlignedMeshAllocator.vao.enableAttribute(1);
	nonAlignedMeshAllocator.vao.setIntAttribute(1, 1, 0, 1, GL_UNSIGNED_INT);
	nonAlignedMeshAllocator.vao.setAttributeDivisor(1, 1);

	// Vertex shifts
	nonAlignedMeshAllocator.vao.enableAttribute(2);
	nonAlignedMeshAllocator.vao.setIntAttribute(2, 2, 1 * sizeof(int), 1, GL_UNSIGNED_INT);
	nonAlignedMeshAllocator.vao.setAttributeDivisor(2, 1);

	// Vs + textureID
	nonAlignedMeshAllocator.vao.enableAttribute(3);
	nonAlignedMeshAllocator.vao.setIntAttribute(3, 1, 3 * sizeof(int), 1, GL_UNSIGNED_INT);
	nonAlignedMeshAllocator.vao.setAttributeDivisor(3, 1);

	// Light
	nonAlignedMeshAllocator.vao.enableAttribute(4);
	nonAlignedMeshAllocator.vao.setIntAttribute(4, 2, 4 * sizeof(int), 1, GL_UNSIGNED_INT);
	nonAlignedMeshAllocator.vao.setAttributeDivisor(4, 1);

	// AO
	nonAlignedMeshAllocator.vao.enableAttribute(5);
	nonAlignedMeshAllocator.vao.setIntAttribute(5, 1, 6 * sizeof(int), 1, GL_UNSIGNED_INT);
	nonAlignedMeshAllocator.vao.setAttributeDivisor(5, 1);
}

ChunkInstancedMeshAllocator& ChunkInstancedMeshAllocator::getInstance()
{
	static ChunkInstancedMeshAllocator instance;
	return instance;
}

void ChunkInstancedMeshAllocator::processMeshAllocationRequests(
	const DynamicArray<ChunkInstancedMeshFaceStorage*>& alignedMeshRequests,
	const DynamicArray<ChunkInstancedMeshFaceStorage*>& nonAlignedMeshRequests
)
{
	alignedMeshAllocator.processMeshRequests(alignedMeshRequests);
	nonAlignedMeshAllocator.processMeshRequests(nonAlignedMeshRequests);
}

ChunkInstancedMeshAllocator::MeshAllocator::MeshAllocator() :
	blockAllocator(0)
{
	vao.create();
	instanceVBO.create(GL_ARRAY_BUFFER);
}

void ChunkInstancedMeshAllocator::MeshAllocator::init(const ProcessorConfig& config)
{
	this->config = config;
}

void ChunkInstancedMeshAllocator::MeshAllocator::processMeshRequests(const DynamicArray<ChunkInstancedMeshFaceStorage*>& meshRequests)
{
	// Store old capacity
	size_t oldCapacity = blockAllocator.getCapacity();

	// Free blocks
	for (ChunkInstancedMeshFaceStorage* chunkMesh : meshRequests)
	{
		if (!config.isCreated(chunkMesh))
		{
			continue;
		}

		ASSERT(blockAllocator.free(config.getAllocatedBlock(chunkMesh).id));
	}

	// Allocate blocks
	size_t stopIndex = 0;
	for (ChunkInstancedMeshFaceStorage* chunkMesh : meshRequests)
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
		ChunkInstancedMeshFaceStorage* chunkMesh = meshRequests[i];
		auto faceCount = config.getFaceCount(chunkMesh);

		auto result = blockAllocator.allocate(faceCount);
		if (!result.has_value())
		{
			std::cerr << "[ChunkInstancedMeshAllocator]: " << config.debugName << ": mesh wasn't created.\n";
			break;
		}

		config.setCreated(chunkMesh, true);
		config.setAllocatedBlock(chunkMesh, result.value());
	}
}
