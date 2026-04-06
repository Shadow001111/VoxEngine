#include "ChunkMesh.h"
#include "../../Chunk.h"

#include "Core/Profiler.h"

#include "ChunkMeshAllocator.h"

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
	DynamicArray<ChunkMeshFaceStorage*> allocateMemoryAlignedMeshRequests;
	DynamicArray<ChunkMeshFaceStorage*> allocateMemoryUnalignedMeshRequests;

	allocateMemoryAlignedMeshRequests.reserve(uploadCount);
	allocateMemoryUnalignedMeshRequests.reserve(uploadCount);

	for (Chunk* chunk : pendingMeshUploads)
	{
		ChunkMeshFaceStorage* chunkMesh = &chunk->mesh.faceStorage;
		if (chunkMesh->getAlignedFaceCount() > chunkMesh->getAlignedFaceCapacity())
		{
			allocateMemoryAlignedMeshRequests.push_back(chunkMesh);
		}
		if (chunkMesh->getUnalignedFaceCount() > chunkMesh->getUnalignedFaceCapacity())
		{
			allocateMemoryUnalignedMeshRequests.push_back(chunkMesh);
		}
	}

	// Allocate memory for meshes
	auto& chunkInstancedMeshAllocator = ChunkMeshAllocator::getInstance();
	chunkInstancedMeshAllocator.processMeshAllocationRequests(allocateMemoryAlignedMeshRequests, allocateMemoryUnalignedMeshRequests);

	// Write meshes data
	auto& alignedInstancesVBO = chunkInstancedMeshAllocator.getAlignedInstanceVBO();
	auto& unalignedInstancesVBO = chunkInstancedMeshAllocator.getUnalignedInstanceVBO();

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

		if (chunkMesh->unalignedCreated)
		{
			auto opaqueFaceCount = chunkMesh->getUnalignedOpaqueFaceCount();
			auto translucentFaceCount = chunkMesh->getUnalignedTranslucentFaceCount();

			if (opaqueFaceCount > 0)
			{
				unalignedInstancesVBO.write(
					chunkMesh->instancesStorage.unalignedOpaque.data(),
					opaqueFaceCount * sizeof(UnalignedBlockFace),
					chunkMesh->allocatedBlock_unalignedFaces.offset * sizeof(UnalignedBlockFace)
				);
			}

			if (translucentFaceCount > 0)
			{
				unalignedInstancesVBO.write(
					chunkMesh->instancesStorage.unalignedTranslucent.data(),
					translucentFaceCount * sizeof(UnalignedBlockFace),
					(chunkMesh->allocatedBlock_unalignedFaces.offset + opaqueFaceCount) * sizeof(UnalignedBlockFace)
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
