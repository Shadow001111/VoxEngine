#include "ChunkMesh.h"
#include "../../Chunk.h"

#include "Core/Profiler.h"

#include "ChunkInstancedMeshAllocator.h"

DynamicArray<Chunk*> ChunkMesh::pendingMeshUploads;

void ChunkMesh::sendMeshesToGPU()
{
	// Check if there are any uploads
	if (pendingMeshUploads.empty())
	{
		return;
	}

	PROFILE_SCOPE("Send chunk meshes to GPU", ProfileCategory::ChunkMesh);

	const size_t uploadCount = pendingMeshUploads.size();

	// Mark meshes as being processed
	for (Chunk* chunk : pendingMeshUploads)
	{
		chunk->mesh.faceStorage.processingFence.startProcessing();
	}

	// Collect meshes that need (re)allocation
	DynamicArray<ChunkInstancedMeshFaceStorage*> allocateMemoryAlignedMeshRequests;
	DynamicArray<ChunkInstancedMeshFaceStorage*> allocateMemoryNonAlignedMeshRequests;

	allocateMemoryAlignedMeshRequests.reserve(uploadCount);
	allocateMemoryNonAlignedMeshRequests.reserve(uploadCount);

	for (Chunk* chunk : pendingMeshUploads)
	{
		ChunkInstancedMeshFaceStorage* chunkMesh = &chunk->mesh.faceStorage;
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

	// Write instances data
	for (Chunk* chunk : pendingMeshUploads)
	{
		auto* chunkMesh = &chunk->mesh.faceStorage;

		if (chunkMesh->alignedCreated)
		{
			auto opaqueFaceCount = chunkMesh->getAlignedOpaqueFaceCount();
			auto translucentFaceCount = chunkMesh->getAlignedTranslucentFaceCount();

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

		if (chunkMesh->nonAlignedCreated)
		{
			auto opaqueFaceCount = chunkMesh->getNonAlignedOpaqueFaceCount();
			auto translucentFaceCount = chunkMesh->getNonAlignedTranslucentFaceCount();

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

		chunkMesh->updateRenderFaceCount();
		chunkMesh->shouldBeUploaded = false;
		chunkMesh->processingFence.stopProcessing();
		chunk->updateCanBeRenderedFlag();
	}

	// Clear pending meshes container
	pendingMeshUploads.clear();
}
