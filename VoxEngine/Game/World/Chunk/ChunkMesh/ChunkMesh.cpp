#include "ChunkMesh.h"
#include "../../Chunk.h"

#include "Core/Profiler.h"

#include "Game/ProfileCategories.h"

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

	// Collect per-layer reallocation requests
	ChunkMeshAllocator::LayerRequests requests;
	for (auto& r : requests)
		r.reserve(uploadCount);

	for (Chunk* chunk : pendingMeshUploads)
	{
		ChunkMeshFaceStorage& faceStorage = chunk->mesh.faceStorage;
		for (int i = 0; i < (int)MeshLayer::Count; i++)
		{
			auto layer = static_cast<MeshLayer>(i);
			if (faceStorage.getFaceCount(layer) > faceStorage.getFaceCapacity(layer))
				requests[i].push_back(&faceStorage);
		}
	}

	// Allocate memory for meshes
	auto& allocator = ChunkMeshAllocator::getInstance();
	allocator.processMeshAllocationRequests(requests);

	// // Upload face data
	for (Chunk* chunk : pendingMeshUploads)
	{
		ChunkMeshFaceStorage& faceStorage = chunk->mesh.faceStorage;

		for (int i = 0; i < (int)MeshLayer::Count; i++)
		{
			auto layer = static_cast<MeshLayer>(i);
			auto faceCount = faceStorage.getFaceCount(layer);

			if (!faceStorage.readFlag(ChunkMeshFaceStorage::createdFlag(layer)) || faceCount == 0)
				continue;

			const size_t stride = faceStructSize(layer);
			allocator.getInstanceVBO(layer).write(
				faceStorage.getFaceData(layer),
				faceCount * stride,
				faceStorage.getFacesBlock(layer).offset * stride
			);
		}

		faceStorage.updateRenderFaceCount();
		faceStorage.setFlag(ChunkMesh::Flag::ShouldBeUploaded, false);
		faceStorage.processingFence.stopProcessing();
		chunk->updateCanBeRenderedFlag();
	}

	// Clear pending meshes container
	pendingMeshUploads.clear();
}
