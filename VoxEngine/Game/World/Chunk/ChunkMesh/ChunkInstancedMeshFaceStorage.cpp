#include "ChunkInstancedMeshFaceStorage.h"

void ChunkInstancedMeshFaceStorage::resetRenderFaceCount()
{
	renderAlignedOpaqueFaceCount = 0;
	renderAlignedTranslucentFaceCount = 0;
	renderNonAlignedOpaqueFaceCount = 0;
	renderNonAlignedTranslucentFaceCount = 0;
}

void ChunkInstancedMeshFaceStorage::updateRenderFaceCount()
{
	renderAlignedOpaqueFaceCount = instancesStorage.alignedOpaque.size();
	renderAlignedTranslucentFaceCount = instancesStorage.alignedTranslucent.size();
	renderNonAlignedOpaqueFaceCount = instancesStorage.nonAlignedOpaque.size();
	renderNonAlignedTranslucentFaceCount = instancesStorage.nonAlignedTranslucent.size();
}

ChunkInstancedMeshFaceStorage::InstancesStorage& ChunkInstancedMeshFaceStorage::InstancesStorage::operator=(InstancesStorage&& other) noexcept
{
	if (this != &other)
	{
		alignedOpaque = std::move(other.alignedOpaque);
		alignedTranslucent = std::move(other.alignedTranslucent);

		nonAlignedOpaque = std::move(other.nonAlignedOpaque);
		nonAlignedTranslucent = std::move(other.nonAlignedTranslucent);
	}
	return *this;
}



void ChunkInstancedMeshFaceStorage::InstancesStorage::clear()
{
	alignedOpaque.clear();
	alignedTranslucent.clear();
	nonAlignedOpaque.clear();
	nonAlignedTranslucent.clear();
}

void ChunkInstancedMeshFaceStorage::InstancesStorage::swap(InstancesStorage& other) noexcept
{
	alignedOpaque.swap(other.alignedOpaque);
	alignedTranslucent.swap(other.alignedTranslucent);
	nonAlignedOpaque.swap(other.nonAlignedOpaque);
	nonAlignedTranslucent.swap(other.nonAlignedTranslucent);
}
