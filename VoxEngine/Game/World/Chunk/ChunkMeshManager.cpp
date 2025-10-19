#include "ChunkMeshManager.h"

#include <iostream>

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
	// I disabled it, because I think it's useless. But who knows.
	constexpr bool ENABLE_REORGANIZATION = false;

	// Store old capacity
	size_t oldCapacity = chunkBlockAllocator.getCapacity();

	if (ENABLE_REORGANIZATION)
	{
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
			else
			{
				allocatedMeshes.erase(chunkMesh->allocatedBlock.id);
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
			allocatedMeshes.emplace(chunkMesh->allocatedBlock.id, chunkMesh);
			stopIndex++;
		}

		if (stopIndex == meshRequests.size())
		{
			return;
		}

		// Organize allocations
		const std::vector<BlockAllocator::Block> oldBlocks = chunkBlockAllocator.getAllAllocations();
		chunkBlockAllocator.organizeAllocations();
		const std::vector<BlockAllocator::Block>& newBlocks = chunkBlockAllocator.getAllAllocations();

		// Update all registered meshes with new offsets
		for (size_t i = 0; i < newBlocks.size(); i++)
		{
			const auto& newBlock = newBlocks[i];
			auto it = allocatedMeshes.find(newBlock.id);
			if (it != allocatedMeshes.end())
			{
				it->second->allocatedBlock = newBlock;
			}
		}

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
			OpenGL_Buffer newBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
			newBuffer.allocateMemory(newCapacity * sizeof(BlockFaceInstance));

			// Copy data to a new buffer
			for (size_t i = 0; i < newBlocks.size(); i++)
			{
				const auto& oldBlock = oldBlocks[i];
				const auto& newBlock = newBlocks[i];

				newBuffer.copyRangeFrom(instanceVBO,
					oldBlock.offset * sizeof(BlockFaceInstance),
					newBlock.offset * sizeof(BlockFaceInstance),
					oldBlock.size * sizeof(BlockFaceInstance)
				);
			}

			// Replace old with new buffer
			instanceVBO = std::move(newBuffer);
			configureInstanceVBO();
		}
		else
		{
			// Create temporary buffer for safe reorganization
			OpenGL_Buffer tempBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
			tempBuffer.allocateMemory(oldCapacity * sizeof(BlockFaceInstance));
			
			// Copy all data to temp buffer first
			for (size_t i = 0; i < oldBlocks.size(); i++)
			{
				const auto& oldBlock = oldBlocks[i];
				tempBuffer.copyRangeFrom(instanceVBO,
					oldBlock.offset * sizeof(BlockFaceInstance),
					oldBlock.offset * sizeof(BlockFaceInstance),
					oldBlock.size   * sizeof(BlockFaceInstance)
				);
			}

			// Copy from temp buffer back to main buffer at new offsets
			for (size_t i = 0; i < newBlocks.size(); i++)
			{
				const auto& oldBlock = oldBlocks[i];
				const auto& newBlock = newBlocks[i];

				instanceVBO.copyRangeFrom(tempBuffer,
					oldBlock.offset * sizeof(BlockFaceInstance),
					newBlock.offset * sizeof(BlockFaceInstance),
					oldBlock.size   * sizeof(BlockFaceInstance)
				);
			}
		}

		// Allocate the rest of blocks
		for (size_t i = stopIndex; i < meshRequests.size(); i++)
		{
			MeshData* chunkMesh = meshRequests[i];

			auto result = chunkBlockAllocator.allocate(chunkMesh->getFaceCount());
			if (!result.has_value())
			{
				std::cout << "[ChunkMeshManager]: processMeshRequests: mesh wasn't created." << std::endl;
				break;
			}

			chunkMesh->created = true;
			chunkMesh->allocatedBlock = result.value();
			allocatedMeshes.emplace(chunkMesh->allocatedBlock.id, chunkMesh);
		}
	}
	else
	{
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
				std::cout << "[ChunkMeshManager]: processMeshRequests: mesh wasn't created." << std::endl;
				break;
			}

			chunkMesh->created = true;
			chunkMesh->allocatedBlock = result.value();
		}
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
