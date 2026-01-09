#include "ChunkMeshManager.h"

#include <iostream>

#include "Core/Assert.h"

ChunkMeshManager::ChunkMeshManager()
{
	// VBO
	const float vertices[8] = // CCW order
	{
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	vbo.create(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
	vbo.bind();
	vbo.allocateMemory(sizeof(vertices));
	vbo.write(vertices, sizeof(vertices));

	{
		ProcessorConfig config = {
			[this]() { configureAlignedInstanceVBO(); },
			[](ChunkMeshData* mesh) { return mesh->getAlignedFaceCount(); },
			[](ChunkMeshData* mesh) { return mesh->alignedCreated; },
			[](ChunkMeshData* mesh) -> BlockAllocator::Block& { return mesh->allocatedBlock_alignedFaces; },
			[](ChunkMeshData* mesh, bool created) { mesh->alignedCreated = created; },
			[](ChunkMeshData* mesh, BlockAllocator::Block block) { mesh->allocatedBlock_alignedFaces = block; },
			sizeof(AlignedBlockFace),
			"processAlignedMeshRequests"
		};
		alignedMeshAllocator.init(config);
	}
	{
		ProcessorConfig config = {
			[this]() { configureNonAlignedInstanceVBO(); },
			[](ChunkMeshData* mesh) { return mesh->getNonAlignedFaceCount(); },
			[](ChunkMeshData* mesh) { return mesh->nonAlignedCreated; },
			[](ChunkMeshData* mesh) -> BlockAllocator::Block& { return mesh->allocatedBlock_nonAlignedFaces; },
			[](ChunkMeshData* mesh, bool created) { mesh->nonAlignedCreated = created; },
			[](ChunkMeshData* mesh, BlockAllocator::Block block) { mesh->allocatedBlock_nonAlignedFaces = block; },
			sizeof(NonAlignedBlockFace),
			"processNonAlignedMeshRequests"
		};
		nonAlignedMeshAllocator.init(config);
	}

	alignedMeshAllocator.vao.bind();
	alignedMeshAllocator.vao.enableAttribute(0);
	alignedMeshAllocator.vao.setFloatAttribute(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	configureAlignedInstanceVBO();

	nonAlignedMeshAllocator.vao.bind();
	nonAlignedMeshAllocator.vao.enableAttribute(0);
	nonAlignedMeshAllocator.vao.setFloatAttribute(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	configureNonAlignedInstanceVBO();
}

ChunkMeshManager::~ChunkMeshManager()
{}

void ChunkMeshManager::configureAlignedInstanceVBO()
{
	alignedMeshAllocator.vao.bind();
	alignedMeshAllocator.instanceVBO.bind();

	alignedMeshAllocator.vao.enableAttribute(1);
	alignedMeshAllocator.vao.setIntAttribute(1, 2, GL_UNSIGNED_INT, sizeof(AlignedBlockFace), (void*)0);
	alignedMeshAllocator.vao.setAttributeDivisor(1, 1);
}

void ChunkMeshManager::configureNonAlignedInstanceVBO()
{
	nonAlignedMeshAllocator.vao.bind();
	nonAlignedMeshAllocator.instanceVBO.bind();

	// Block position + Us
	nonAlignedMeshAllocator.vao.enableAttribute(1);
	nonAlignedMeshAllocator.vao.setIntAttribute(1, 1, GL_UNSIGNED_INT, sizeof(NonAlignedBlockFace), (void*)0);
	nonAlignedMeshAllocator.vao.setAttributeDivisor(1, 1);

	// Vertex shifts
	nonAlignedMeshAllocator.vao.enableAttribute(2);
	nonAlignedMeshAllocator.vao.setIntAttribute(2, 2, GL_UNSIGNED_INT, sizeof(NonAlignedBlockFace), (void*)(1 * sizeof(int)));
	nonAlignedMeshAllocator.vao.setAttributeDivisor(2, 1);

	// Vs + textureID
	nonAlignedMeshAllocator.vao.enableAttribute(3);
	nonAlignedMeshAllocator.vao.setIntAttribute(3, 1, GL_UNSIGNED_INT, sizeof(NonAlignedBlockFace), (void*)(3 * sizeof(int)));
	nonAlignedMeshAllocator.vao.setAttributeDivisor(3, 1);

	// Light
	nonAlignedMeshAllocator.vao.enableAttribute(4);
	nonAlignedMeshAllocator.vao.setIntAttribute(4, 2, GL_UNSIGNED_INT, sizeof(NonAlignedBlockFace), (void*)(4 * sizeof(int)));
	nonAlignedMeshAllocator.vao.setAttributeDivisor(4, 1);

	// AO
	nonAlignedMeshAllocator.vao.enableAttribute(5);
	nonAlignedMeshAllocator.vao.setIntAttribute(5, 1, GL_UNSIGNED_INT, sizeof(NonAlignedBlockFace), (void*)(6 * sizeof(int)));
	nonAlignedMeshAllocator.vao.setAttributeDivisor(5, 1);
}

ChunkMeshManager& ChunkMeshManager::getInstance()
{
	static ChunkMeshManager instance;
	return instance;
}

void ChunkMeshManager::processMeshRequests(std::vector<ChunkMeshData*>& alignedMeshRequests, std::vector<ChunkMeshData*>& nonAlignedMeshRequests)
{
	alignedMeshAllocator.processMeshRequests(alignedMeshRequests);
	nonAlignedMeshAllocator.processMeshRequests(nonAlignedMeshRequests);
}

ChunkMeshManager::MeshAllocator::MeshAllocator() :
	blockAllocator(0)
{
	vao.create();
	instanceVBO.create(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
}

ChunkMeshManager::MeshAllocator::~MeshAllocator()
{}

void ChunkMeshManager::MeshAllocator::init(const ProcessorConfig& config)
{
	this->config = config;
}

void ChunkMeshManager::MeshAllocator::processMeshRequests(std::vector<ChunkMeshData*>& meshRequests)
{
	// Store old capacity
	size_t oldCapacity = blockAllocator.getCapacity();

	// Free blocks
	for (ChunkMeshData* chunkMesh : meshRequests)
	{
		if (!chunkMesh->nonAlignedCreated)
		{
			continue;
		}

		ASSERT(blockAllocator.free(config.getAllocatedBlock(chunkMesh).id));
	}

	// Allocate blocks
	size_t stopIndex = 0;
	for (ChunkMeshData* chunkMesh : meshRequests)
	{
		size_t faceCount = config.getFaceCount(chunkMesh);

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
		instanceVBO.allocateMemory(newCapacity * config.faceSize);
	}
	else
	{
		// Create new buffer
		OpenGL_Buffer newBuffer;
		newBuffer.create(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
		newBuffer.bind();
		newBuffer.allocateMemory(newCapacity * config.faceSize);

		// Copy data to a new buffer
		const std::vector<BlockAllocator::Block>& currentBlocks = blockAllocator.getAllAllocations();
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
		ChunkMeshData* chunkMesh = meshRequests[i];
		size_t faceCount = config.getFaceCount(chunkMesh);

		auto result = blockAllocator.allocate(faceCount);
		if (!result.has_value())
		{
			std::cerr << "[ChunkMeshManager]: " << config.debugName << ": mesh wasn't created.\n";
			break;
		}

		config.setCreated(chunkMesh, true);
		config.setAllocatedBlock(chunkMesh, result.value());
	}
}
