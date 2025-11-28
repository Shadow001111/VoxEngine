#include "ChunkMeshManager.h"

#include <iostream>

#include "Core/Assert.h"

ChunkMeshManager::ChunkMeshManager() :
	vbo(GL_ARRAY_BUFFER, GL_STATIC_DRAW)
{
	// VBO
	float vertices[8] = // CCW order
	{
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	vbo.bind();
	vbo.allocateMemory_optionalBind(sizeof(vertices));
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
		alignedMeshAllocator.init(vbo, config);
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
		nonAlignedMeshAllocator.init(vbo, config);
	}

	glBindVertexArray(alignedMeshAllocator.vaoID);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	glBindVertexArray(nonAlignedMeshAllocator.vaoID);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	configureAlignedInstanceVBO();
	configureNonAlignedInstanceVBO();
}

ChunkMeshManager::~ChunkMeshManager()
{}

void ChunkMeshManager::configureAlignedInstanceVBO()
{
	glBindVertexArray(alignedMeshAllocator.vaoID);
	alignedMeshAllocator.instanceVBO.bind();

	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 2, GL_INT, sizeof(AlignedBlockFace), (void*)0);
	glVertexAttribDivisor(1, 1);
}

void ChunkMeshManager::configureNonAlignedInstanceVBO()
{
	glBindVertexArray(nonAlignedMeshAllocator.vaoID);
	nonAlignedMeshAllocator.instanceVBO.bind();

	// Block position + Us
	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 1, GL_INT, sizeof(NonAlignedBlockFace), (void*)0);
	glVertexAttribDivisor(1, 1);

	// Vertex shifts
	glEnableVertexAttribArray(2);
	glVertexAttribIPointer(2, 2, GL_INT, sizeof(NonAlignedBlockFace), (void*)(1 * sizeof(int)));
	glVertexAttribDivisor(2, 1);

	// Vs + textureID
	glEnableVertexAttribArray(3);
	glVertexAttribIPointer(3, 1, GL_INT, sizeof(NonAlignedBlockFace), (void*)(3 * sizeof(int)));
	glVertexAttribDivisor(3, 1);

	// Light
	glEnableVertexAttribArray(4);
	glVertexAttribIPointer(4, 2, GL_INT, sizeof(NonAlignedBlockFace), (void*)(4 * sizeof(int)));
	glVertexAttribDivisor(4, 1);

	// AO
	glEnableVertexAttribArray(5);
	glVertexAttribIPointer(5, 1, GL_INT, sizeof(NonAlignedBlockFace), (void*)(6 * sizeof(int)));
	glVertexAttribDivisor(5, 1);
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

void ChunkMeshManager::bindAlignedVAO() const
{
	glBindVertexArray(alignedMeshAllocator.vaoID);
}

void ChunkMeshManager::bindNonAlignedVAO() const
{
	glBindVertexArray(nonAlignedMeshAllocator.vaoID);
}

ChunkMeshManager::MeshAllocator::MeshAllocator() :
	vaoID(0), instanceVBO(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW), blockAllocator(0)
{
}

ChunkMeshManager::MeshAllocator::~MeshAllocator()
{
	if (vaoID)
	{
		glDeleteVertexArrays(1, &vaoID); vaoID = 0;
	}
}

void ChunkMeshManager::MeshAllocator::init(const OpenGL_Buffer& quadVBO, const ProcessorConfig& config)
{
	glGenVertexArrays(1, &vaoID);
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
		instanceVBO.allocateMemory_optionalBind(newCapacity * config.faceSize);
	}
	else
	{
		// Create new buffer
		OpenGL_Buffer newBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
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
			std::cerr << "[ChunkMeshManager]: " << config.debugName << ": mesh wasn't created." << std::endl;
			break;
		}

		config.setCreated(chunkMesh, true);
		config.setAllocatedBlock(chunkMesh, result.value());
	}
}
