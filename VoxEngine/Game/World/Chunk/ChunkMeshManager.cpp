#include "ChunkMeshManager.h"

#include <iostream>

#include "Core/Debug.h"

ChunkMeshManager::ChunkMeshManager() :
	vao(0), vbo(0), instanceVBO(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW), chunkBlockAllocator(0)
{
	// Create buffers once
	float vertices[8] = // CCW order
	{
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	OPENGL_LOG_BUFFER_CREATED(1, &vbo);

	// Bind VAO
	glBindVertexArray(vao);

	// Vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	// Instance buffer
	configureInstanceVBO();
}

ChunkMeshManager::~ChunkMeshManager()
{
	if (vbo)
	{
		glDeleteBuffers(1, &vbo); vbo = 0;
	}
	if (vao)
	{
		glDeleteVertexArrays(1, &vao); vao = 0;
	}
}

void ChunkMeshManager::configureInstanceVBO()
{
	glBindVertexArray(vao);
	instanceVBO.bind();

	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 2, GL_INT, sizeof(BlockFaceInstance), (void*)0); // integer attribute
	glVertexAttribDivisor(1, 1); // advance per instance
}

ChunkMeshManager& ChunkMeshManager::getInstance()
{
	static ChunkMeshManager instance;
	return instance;
}

void ChunkMeshManager::preallocateMemory(size_t instanceCount)
{
	instanceVBO.allocateMemory(instanceCount * sizeof(BlockFaceInstance));
	if (!chunkBlockAllocator.setCapacity(instanceCount))
	{
		throw std::runtime_error("[GlobalChunkMesh]: Pre-allocate memory: chunkBlockAllocator couldn't shrink data.");
	}
}

void ChunkMeshManager::processMeshRequests(std::vector<MeshData*>& meshRequests)
{
	// Store old capacity
	size_t oldCapacity = chunkBlockAllocator.getCapacity();
	
	// Free blocks
	for (MeshData* chunkMesh : meshRequests)
	{
		if (!chunkMesh->created)
		{
			continue;
		}

		// Shouldn't be happening
		if (!chunkBlockAllocator.free(chunkMesh->allocatedBlock.id))
		{
			std::cerr << "[ChunkMeshManager] Block was already freed." << std::endl;
		}
	}

	// Allocate blocks
	size_t stopIndex = 0;
	for (MeshData* chunkMesh : meshRequests)
	{
		auto result = chunkBlockAllocator.allocate(chunkMesh->getFaceCount());
		if (!result.has_value())
		{
			break;
		}

		chunkMesh->created = true;
		chunkMesh->allocatedBlock = result.value();
		stopIndex++;
	}

	if (stopIndex == meshRequests.size())
	{
		return;
	}

	// Organize allocations
	const std::vector<BlockAllocator::Block>& currentBlocks = chunkBlockAllocator.getAllAllocations();

	// Allocate more memory
	size_t newCapacity = chunkBlockAllocator.getLastBlockEnd();
	for (size_t i = stopIndex; i < meshRequests.size(); i++)
	{
		MeshData* chunkMesh = meshRequests[i];
		newCapacity += chunkMesh->getFaceCount();
	}

	bool needNewBuffer = newCapacity > oldCapacity;
	if (needNewBuffer)
	{
		newCapacity += (newCapacity >> 1); // *= 1.5
	}

	chunkBlockAllocator.setCapacity(newCapacity);

	if (oldCapacity == 0)
	{
		instanceVBO.allocateMemory(newCapacity * sizeof(BlockFaceInstance));
	}
	else if (needNewBuffer)
	{
		// Create new buffer
		// TODO: Debug message (131186): Buffer performance warning: Buffer object 11 (bound to GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB (1), GL_ARRAY_BUFFER_ARB, and GL_COPY_WRITE_BUFFER_BINDING_EXT, usage hint is GL_DYNAMIC_DRAW) is being copied/moved from VIDEO memory to HOST memory.
		OpenGL_Buffer newBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
		newBuffer.allocateMemory(newCapacity * sizeof(BlockFaceInstance));

		// Copy data to a new buffer
		for (const auto& block : currentBlocks)
		{
			newBuffer.copyRangeFrom(instanceVBO,
				block.offset * sizeof(BlockFaceInstance),
				block.offset * sizeof(BlockFaceInstance),
				block.size * sizeof(BlockFaceInstance)
			);
		}

		// Replace old with new buffer
		instanceVBO = std::move(newBuffer);
		configureInstanceVBO();
	}

	// Allocate the rest of blocks
	for (size_t i = stopIndex; i < meshRequests.size(); i++)
	{
		MeshData* chunkMesh = meshRequests[i];

		auto result = chunkBlockAllocator.allocate(chunkMesh->getFaceCount());
		if (!result.has_value())
		{
			std::cerr << "[ChunkMeshManager]: processMeshRequests: mesh wasn't created." << std::endl;
			break;
		}

		chunkMesh->created = true;
		chunkMesh->allocatedBlock = result.value();
	}
}

OpenGL_Buffer& ChunkMeshManager::getInstanceVBO()
{
	return instanceVBO;
}

void ChunkMeshManager::bindVAO() const
{
	glBindVertexArray(vao);
}

size_t ChunkMeshManager::getGaps() const
{
	return chunkBlockAllocator.getGapSizesSum();
}
