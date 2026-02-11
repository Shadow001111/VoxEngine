#include "ChunkMesh.h"

#include "Core/Profiler.h"

#include "ChunkInstancedMeshAllocator.h"

std::vector<ChunkInstancedMeshFaceStorage*> ChunkMesh::pendingMeshUploads;
std::atomic<bool> ChunkMesh::hasPendingMeshUploads{ false };

void ChunkMesh::sendMeshesToGPU()
{
	// Check if there are any uploads
	if (pendingMeshUploads.empty())
	{
		return;
	}

	PROFILE_SCOPE("Send chunk meshes to GPU", ProfileCategory::ChunkMesh);

	// Mark meshes as being processed
	for (ChunkInstancedMeshFaceStorage* chunkMesh : pendingMeshUploads)
	{
		chunkMesh->processingFence.startProcessing();
	}

	// Collect meshes that need memory allocation
	std::vector<ChunkInstancedMeshFaceStorage*> allocateMemoryAlignedMeshRequests;
	allocateMemoryAlignedMeshRequests.reserve(pendingMeshUploads.size() >> 1);

	std::vector<ChunkInstancedMeshFaceStorage*> allocateMemoryNonAlignedMeshRequests;
	allocateMemoryNonAlignedMeshRequests.reserve(pendingMeshUploads.size() >> 1);

	for (ChunkInstancedMeshFaceStorage* chunkMesh : pendingMeshUploads)
	{
		if (chunkMesh->getAlignedFaceCount() > chunkMesh->getAlignedFaceCapacity())
		{
			allocateMemoryAlignedMeshRequests.push_back(chunkMesh);
		}
		if (chunkMesh->getNonAlignedFaceCount() > chunkMesh->getNonAlignedFaceCapacity())
		{
			allocateMemoryNonAlignedMeshRequests.push_back(chunkMesh);
		}
	}

	// Allocate memory for meshes
	auto& chunkInstancedMeshAllocator = ChunkInstancedMeshAllocator::getInstance();
	chunkInstancedMeshAllocator.processMeshAllocationRequests(allocateMemoryAlignedMeshRequests, allocateMemoryNonAlignedMeshRequests);

	// Write meshes data
	auto& alignedInstancesVBO = chunkInstancedMeshAllocator.getAlignedInstanceVBO();
	auto& nonAlignedInstancesVBO = chunkInstancedMeshAllocator.getNonAlignedInstanceVBO();

	// Potential bug: If mesh.meshData.opaqueDirty and it will require more data then it was previously, then it will overwrite previous transparent data
	// But for now, both opaque and transparent parts are dirty, so this bug won't happen

	// Write aligned instances data
	for (ChunkInstancedMeshFaceStorage* chunkMesh : pendingMeshUploads)
	{
		if (!chunkMesh->alignedCreated)
		{
			continue;
		}

		size_t opaqueFaceCount = chunkMesh->getAlignedOpaqueFaceCount();
		size_t translucentFaceCount = chunkMesh->getAlignedTranslucentFaceCount();

		if (opaqueFaceCount > 0)
		{
			alignedInstancesVBO.write(
				chunkMesh->instancesStorage.alignedOpaque.data(),
				opaqueFaceCount * sizeof(AlignedBlockFace),
				chunkMesh->allocatedBlock_alignedFaces.offset * sizeof(AlignedBlockFace)
			);
		}

		if (translucentFaceCount > 0)
		{
			alignedInstancesVBO.write(
				chunkMesh->instancesStorage.alignedTranslucent.data(),
				translucentFaceCount * sizeof(AlignedBlockFace),
				(chunkMesh->allocatedBlock_alignedFaces.offset + opaqueFaceCount) * sizeof(AlignedBlockFace)
			);
		}
	}

	// Write aligned instances data
	for (ChunkInstancedMeshFaceStorage* chunkMesh : pendingMeshUploads)
	{
		if (!chunkMesh->nonAlignedCreated)
		{
			continue;
		}

		size_t opaqueFaceCount = chunkMesh->getNonAlignedOpaqueFaceCount();
		size_t translucentFaceCount = chunkMesh->getNonAlignedTranslucentFaceCount();

		if (opaqueFaceCount > 0)
		{
			nonAlignedInstancesVBO.write(
				chunkMesh->instancesStorage.nonAlignedOpaque.data(),
				opaqueFaceCount * sizeof(NonAlignedBlockFace),
				chunkMesh->allocatedBlock_nonAlignedFaces.offset * sizeof(NonAlignedBlockFace)
			);
		}

		if (translucentFaceCount > 0)
		{
			nonAlignedInstancesVBO.write(
				chunkMesh->instancesStorage.nonAlignedTranslucent.data(),
				translucentFaceCount * sizeof(NonAlignedBlockFace),
				(chunkMesh->allocatedBlock_nonAlignedFaces.offset + opaqueFaceCount) * sizeof(NonAlignedBlockFace)
			);
		}
	}

	// 
	for (ChunkInstancedMeshFaceStorage* chunkMesh : pendingMeshUploads)
	{
		chunkMesh->updateRenderFaceCount();
		chunkMesh->dirty = false;
		//chunkMesh->clearInstances(); // Can be cleared, but it won't change anything.
		chunkMesh->processingFence.stopProcessing();
	}

	// Clear pending meshes container
	pendingMeshUploads.clear();
}
