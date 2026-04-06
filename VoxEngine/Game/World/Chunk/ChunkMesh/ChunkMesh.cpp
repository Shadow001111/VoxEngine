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
	DynamicArray<ChunkMeshFaceStorage*> allocateMemoryAlignedOpaqueMeshRequests;
	DynamicArray<ChunkMeshFaceStorage*> allocateMemoryAlignedTranslucentMeshRequests;
	DynamicArray<ChunkMeshFaceStorage*> allocateMemoryUnalignedOpaqueMeshRequests;
	DynamicArray<ChunkMeshFaceStorage*> allocateMemoryUnalignedTranslucentMeshRequests;

	allocateMemoryAlignedOpaqueMeshRequests.reserve(uploadCount);
	allocateMemoryAlignedTranslucentMeshRequests.reserve(uploadCount);
	allocateMemoryUnalignedOpaqueMeshRequests.reserve(uploadCount);
	allocateMemoryUnalignedTranslucentMeshRequests.reserve(uploadCount);

	for (Chunk* chunk : pendingMeshUploads)
	{
		ChunkMeshFaceStorage* chunkMesh = &chunk->mesh.faceStorage;
		if (chunkMesh->getAlignedOpaqueFaceCount() > chunkMesh->getAlignedOpaqueFaceCapacity())
		{
			allocateMemoryAlignedOpaqueMeshRequests.push_back(chunkMesh);
		}
		if (chunkMesh->getAlignedTranslucentFaceCount() > chunkMesh->getAlignedTranslucentFaceCapacity())
		{
			allocateMemoryAlignedTranslucentMeshRequests.push_back(chunkMesh);
		}
		if (chunkMesh->getUnalignedOpaqueFaceCount() > chunkMesh->getUnalignedOpaqueFaceCapacity())
		{
			allocateMemoryUnalignedOpaqueMeshRequests.push_back(chunkMesh);
		}
		if (chunkMesh->getUnalignedTranslucentFaceCount() > chunkMesh->getUnalignedTranslucentFaceCapacity())
		{
			allocateMemoryUnalignedTranslucentMeshRequests.push_back(chunkMesh);
		}
	}

	// Allocate memory for meshes
	auto& chunkInstancedMeshAllocator = ChunkMeshAllocator::getInstance();
	chunkInstancedMeshAllocator.processMeshAllocationRequests(
		allocateMemoryAlignedOpaqueMeshRequests,
		allocateMemoryAlignedTranslucentMeshRequests,
		allocateMemoryUnalignedOpaqueMeshRequests,
		allocateMemoryUnalignedTranslucentMeshRequests
	);

	// Write meshes data
	auto& alignedOpaqueInstancesVBO = chunkInstancedMeshAllocator.getAlignedOpaqueInstanceVBO();
	auto& alignedTranslucentInstancesVBO = chunkInstancedMeshAllocator.getAlignedTranslucentInstanceVBO();
	auto& unalignedOpaqueInstancesVBO = chunkInstancedMeshAllocator.getUnalignedOpaqueInstanceVBO();
	auto& unalignedTranslucentInstancesVBO = chunkInstancedMeshAllocator.getUnalignedTranslucentInstanceVBO();

	// Write instances data
	for (Chunk* chunk : pendingMeshUploads)
	{
		auto* chunkMesh = &chunk->mesh.faceStorage;

		if (chunkMesh->readFlag(ChunkMesh::Flag::AlignedOpaqueCreated))
		{
			const auto faceCount = chunkMesh->getAlignedOpaqueFaceCount();
			if (faceCount > 0)
			{
				constexpr size_t faceStructSize = sizeof(AlignedBlockFace);
				alignedOpaqueInstancesVBO.write(
					chunkMesh->instancesStorage.alignedOpaque.data(),
					faceCount * faceStructSize,
					chunkMesh->alignedOpaqueFacesBlock.offset * faceStructSize
				);
			}
		}
		if (chunkMesh->readFlag(ChunkMesh::Flag::AlignedTranslucentCreated))
		{
			const auto faceCount = chunkMesh->getAlignedTranslucentFaceCount();
			if (faceCount > 0)
			{
				constexpr size_t faceStructSize = sizeof(AlignedBlockFace);
				alignedTranslucentInstancesVBO.write(
					chunkMesh->instancesStorage.alignedTranslucent.data(),
					faceCount * faceStructSize,
					chunkMesh->alignedTranslucentFacesBlock.offset * faceStructSize
				);
			}
		}
		if (chunkMesh->readFlag(ChunkMesh::Flag::UnalignedOpaqueCreated))
		{
			const auto faceCount = chunkMesh->getUnalignedOpaqueFaceCount();
			if (faceCount > 0)
			{
				constexpr size_t faceStructSize = sizeof(UnalignedBlockFace);
				unalignedOpaqueInstancesVBO.write(
					chunkMesh->instancesStorage.unalignedOpaque.data(),
					faceCount * faceStructSize,
					chunkMesh->unalignedOpaqueFacesBlock.offset * faceStructSize
				);
			}
		}
		if (chunkMesh->readFlag(ChunkMesh::Flag::UnalignedTranslucentCreated))
		{
			const auto faceCount = chunkMesh->getUnalignedTranslucentFaceCount();
			if (faceCount > 0)
			{
				constexpr size_t faceStructSize = sizeof(UnalignedBlockFace);
				unalignedTranslucentInstancesVBO.write(
					chunkMesh->instancesStorage.unalignedTranslucent.data(),
					faceCount * faceStructSize,
					chunkMesh->unalignedTranslucentFacesBlock.offset * faceStructSize
				);
			}
		}

		chunkMesh->updateRenderFaceCount();
		chunkMesh->setFlag(ChunkMesh::Flag::ShouldBeUploaded, false);
		chunkMesh->processingFence.stopProcessing();
		chunk->updateCanBeRenderedFlag();
	}

	// Clear pending meshes container
	pendingMeshUploads.clear();
}
