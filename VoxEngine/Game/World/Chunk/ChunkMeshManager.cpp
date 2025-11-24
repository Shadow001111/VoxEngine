#include "ChunkMeshManager.h"

#include <iostream>

#include "Core/Assert.h"

ChunkMeshManager::ChunkMeshManager() :
	alignedVAO(0), nonAlignedVAO(0),
	vbo(GL_ARRAY_BUFFER, GL_STATIC_DRAW),
	alignedInstanceVBO(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW), nonAlignedInstanceVBO(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW),
	alignedBlockAllocator(0), nonAlignedBlockAllocator(0)
{
	// Create buffers once
	float vertices[8] = // CCW order
	{
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	glGenVertexArrays(1, &alignedVAO);
	glGenVertexArrays(1, &nonAlignedVAO);

	vbo.bind();
	vbo.allocateMemory(sizeof(vertices));
	vbo.write(vertices, sizeof(vertices));

	glBindVertexArray(alignedVAO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	glBindVertexArray(nonAlignedVAO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
}

ChunkMeshManager::~ChunkMeshManager()
{
	if (alignedVAO)
	{
		glDeleteVertexArrays(1, &alignedVAO); alignedVAO = 0;
	}
	if (nonAlignedVAO)
	{
		glDeleteVertexArrays(1, &nonAlignedVAO); nonAlignedVAO = 0;
	}
}

void ChunkMeshManager::configureAlignedInstanceVBO()
{
	glBindVertexArray(alignedVAO);
	alignedInstanceVBO.bind();

	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 2, GL_INT, sizeof(AlignedBlockFace), (void*)0);
	glVertexAttribDivisor(1, 1);
}

void ChunkMeshManager::configureNonAlignedInstanceVBO()
{
	glBindVertexArray(nonAlignedVAO);
	nonAlignedInstanceVBO.bind();

	// Positions
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(NonAlignedBlockFace), (void*)(0 * sizeof(float)));
	glVertexAttribDivisor(1, 1);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(NonAlignedBlockFace), (void*)(3 * sizeof(float)));
	glVertexAttribDivisor(2, 1);

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(NonAlignedBlockFace), (void*)(6 * sizeof(float)));
	glVertexAttribDivisor(3, 1);

	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(NonAlignedBlockFace), (void*)(9 * sizeof(float)));
	glVertexAttribDivisor(4, 1);

	// UVs
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(NonAlignedBlockFace), (void*)(12 * sizeof(float)));
	glVertexAttribDivisor(5, 1);

	glEnableVertexAttribArray(6);
	glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, sizeof(NonAlignedBlockFace), (void*)(14 * sizeof(float)));
	glVertexAttribDivisor(6, 1);

	glEnableVertexAttribArray(7);
	glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, sizeof(NonAlignedBlockFace), (void*)(16 * sizeof(float)));
	glVertexAttribDivisor(7, 1);

	glEnableVertexAttribArray(8);
	glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, sizeof(NonAlignedBlockFace), (void*)(18 * sizeof(float)));
	glVertexAttribDivisor(8, 1);

	// Texture ID
	glEnableVertexAttribArray(9);
	glVertexAttribIPointer(9, 1, GL_INT, sizeof(NonAlignedBlockFace), (void*)(20 * sizeof(float)));
	glVertexAttribDivisor(9, 1);
}

ChunkMeshManager& ChunkMeshManager::getInstance()
{
	static ChunkMeshManager instance;
	return instance;
}

//void ChunkMeshManager::preallocateAlignedMemory(size_t instanceCount)
//{
//	alignedInstanceVBO.allocateMemory(instanceCount * sizeof(AlignedBlockFace));
//	if (!alignedChunkBlockAllocator.setCapacity(instanceCount))
//	{
//		throw std::runtime_error("[GlobalChunkMesh]: Pre-allocate memory: chunkBlockAllocator couldn't shrink data.");
//	}
//}

void ChunkMeshManager::processMeshRequests(std::vector<ChunkMeshData*>& meshRequests)
{
	processAlignedMeshRequests(meshRequests);
	processNonAlignedMeshRequests(meshRequests);
}

void ChunkMeshManager::processAlignedMeshRequests(std::vector<ChunkMeshData*>& meshRequests)
{
	// Store old capacity
	size_t oldCapacity = alignedBlockAllocator.getCapacity();

	// Free blocks
	for (ChunkMeshData* chunkMesh : meshRequests)
	{
		if (!chunkMesh->alignedCreated || chunkMesh->getAlignedFaceCount() == 0)
		{
			continue;
		}

		ASSERT(alignedBlockAllocator.free(chunkMesh->allocatedBlock_alignedFaces.id));
	}

	// Allocate blocks
	size_t stopIndex = 0;
	for (ChunkMeshData* chunkMesh : meshRequests)
	{
		size_t faceCount = chunkMesh->getAlignedFaceCount();
		if (faceCount == 0)
		{
			continue;
		}
		
		auto result = alignedBlockAllocator.allocate(faceCount);
		if (!result.has_value())
		{
			break;
		}

		chunkMesh->alignedCreated = true;
		chunkMesh->allocatedBlock_alignedFaces = result.value();
		stopIndex++;
	}

	if (stopIndex == meshRequests.size())
	{
		return;
	}

	// Allocate more memory
	size_t newCapacity = alignedBlockAllocator.getLastBlockEnd();
	for (size_t i = stopIndex; i < meshRequests.size(); i++)
	{
		ChunkMeshData* chunkMesh = meshRequests[i];
		newCapacity += chunkMesh->getAlignedFaceCount();
	}

	bool needNewBuffer = newCapacity > oldCapacity;
	if (needNewBuffer)
	{
		newCapacity += (newCapacity >> 1); // *= 1.5
	}

	alignedBlockAllocator.setCapacity(newCapacity);

	if (needNewBuffer)
	{
		if (oldCapacity == 0)
		{
			alignedInstanceVBO.allocateMemory(newCapacity * sizeof(AlignedBlockFace));
		}
		else
		{
			// Create new buffer
			// TODO: Debug message (131186): Buffer performance warning: Buffer object 11 (bound to GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB (1), GL_ARRAY_BUFFER_ARB, and GL_COPY_WRITE_BUFFER_BINDING_EXT, usage hint is GL_DYNAMIC_DRAW) is being copied/moved from VIDEO memory to HOST memory.
			OpenGL_Buffer newBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
			newBuffer.allocateMemory(newCapacity * sizeof(AlignedBlockFace));

			// Copy data to a new buffer
			const std::vector<BlockAllocator::Block>& currentBlocks = alignedBlockAllocator.getAllAllocations();
			for (const auto& block : currentBlocks)
			{
				newBuffer.copyRangeFrom(alignedInstanceVBO,
					block.offset * sizeof(AlignedBlockFace),
					block.offset * sizeof(AlignedBlockFace),
					block.size * sizeof(AlignedBlockFace)
				);
			}

			// Replace old with new buffer
			alignedInstanceVBO = std::move(newBuffer);
			configureAlignedInstanceVBO();
		}
	}

	// Allocate the rest of blocks
	for (size_t i = stopIndex; i < meshRequests.size(); i++)
	{
		ChunkMeshData* chunkMesh = meshRequests[i];
		size_t faceCount = chunkMesh->getAlignedFaceCount();
		if (faceCount == 0)
		{
			continue;
		}

		auto result = alignedBlockAllocator.allocate(faceCount);
		if (!result.has_value())
		{
			std::cerr << "[ChunkMeshManager]: processAlignedMeshRequests: mesh wasn't created." << std::endl;
			break;
		}

		chunkMesh->alignedCreated = true;
		chunkMesh->allocatedBlock_alignedFaces = result.value();
	}
}

void ChunkMeshManager::processNonAlignedMeshRequests(std::vector<ChunkMeshData*>& meshRequests)
{
	// Store old capacity
	size_t oldCapacity = nonAlignedBlockAllocator.getCapacity();

	// Free blocks
	for (ChunkMeshData* chunkMesh : meshRequests)
	{
		if (!chunkMesh->nonAlignedCreated || chunkMesh->getNonAlignedFaceCount() == 0)
		{
			continue;
		}

		ASSERT(nonAlignedBlockAllocator.free(chunkMesh->allocatedBlock_nonAlignedFaces.id));
	}

	// Allocate blocks
	size_t stopIndex = 0;
	for (ChunkMeshData* chunkMesh : meshRequests)
	{
		size_t faceCount = chunkMesh->getNonAlignedFaceCount();
		if (faceCount == 0)
		{
			continue;
		}

		auto result = nonAlignedBlockAllocator.allocate(faceCount);
		if (!result.has_value())
		{
			break;
		}

		chunkMesh->nonAlignedCreated = true;
		chunkMesh->allocatedBlock_nonAlignedFaces = result.value();
		stopIndex++;
	}

	if (stopIndex == meshRequests.size())
	{
		return;
	}

	// Allocate more memory
	size_t newCapacity = nonAlignedBlockAllocator.getLastBlockEnd();
	for (size_t i = stopIndex; i < meshRequests.size(); i++)
	{
		ChunkMeshData* chunkMesh = meshRequests[i];
		newCapacity += chunkMesh->getNonAlignedFaceCount();
	}

	bool needNewBuffer = newCapacity > oldCapacity;
	if (needNewBuffer)
	{
		newCapacity += (newCapacity >> 1); // *= 1.5
	}

	nonAlignedBlockAllocator.setCapacity(newCapacity);

	if (needNewBuffer)
	{
		if (oldCapacity == 0)
		{
			nonAlignedInstanceVBO.allocateMemory(newCapacity * sizeof(NonAlignedBlockFace));
		}
		else
		{
			// Create new buffer
			// TODO: Debug message (131186): Buffer performance warning: Buffer object 11 (bound to GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB (1), GL_ARRAY_BUFFER_ARB, and GL_COPY_WRITE_BUFFER_BINDING_EXT, usage hint is GL_DYNAMIC_DRAW) is being copied/moved from VIDEO memory to HOST memory.
			OpenGL_Buffer newBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
			newBuffer.allocateMemory(newCapacity * sizeof(NonAlignedBlockFace));

			// Copy data to a new buffer
			const std::vector<BlockAllocator::Block>& currentBlocks = nonAlignedBlockAllocator.getAllAllocations();
			for (const auto& block : currentBlocks)
			{
				newBuffer.copyRangeFrom(nonAlignedInstanceVBO,
					block.offset * sizeof(NonAlignedBlockFace),
					block.offset * sizeof(NonAlignedBlockFace),
					block.size * sizeof(NonAlignedBlockFace)
				);
			}

			// Replace old with new buffer
			nonAlignedInstanceVBO = std::move(newBuffer);
			configureNonAlignedInstanceVBO();
		}
	}

	// Allocate the rest of blocks
	for (size_t i = stopIndex; i < meshRequests.size(); i++)
	{
		ChunkMeshData* chunkMesh = meshRequests[i];
		size_t faceCount = chunkMesh->getNonAlignedFaceCount();
		if (faceCount == 0)
		{
			continue;
		}

		auto result = nonAlignedBlockAllocator.allocate(faceCount);
		if (!result.has_value())
		{
			std::cerr << "[ChunkMeshManager]: processNonAlignedMeshRequests: mesh wasn't created." << std::endl;
			break;
		}

		chunkMesh->nonAlignedCreated = true;
		chunkMesh->allocatedBlock_nonAlignedFaces = result.value();
	}
}

void ChunkMeshManager::bindAlignedVAO() const
{
	glBindVertexArray(alignedVAO);
}

void ChunkMeshManager::bindNonAlignedVAO() const
{
	glBindVertexArray(nonAlignedVAO);
}
