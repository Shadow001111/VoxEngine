#include "ChunkMeshFaceStorage.h"

void ChunkMeshFaceStorage::resetRenderFaceCount()
{
	renderAlignedOpaqueFaceCount = 0;
	renderAlignedTranslucentFaceCount = 0;
	renderNonAlignedOpaqueFaceCount = 0;
	renderNonAlignedTranslucentFaceCount = 0;
}

void ChunkMeshFaceStorage::updateRenderFaceCount()
{
	renderAlignedOpaqueFaceCount = instancesStorage.alignedOpaque.size();
	renderAlignedTranslucentFaceCount = instancesStorage.alignedTranslucent.size();
	renderNonAlignedOpaqueFaceCount = instancesStorage.nonAlignedOpaque.size();
	renderNonAlignedTranslucentFaceCount = instancesStorage.nonAlignedTranslucent.size();
}

ChunkMeshFaceStorage::InstancesStorage& ChunkMeshFaceStorage::InstancesStorage::operator=(InstancesStorage&& other) noexcept
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



void ChunkMeshFaceStorage::InstancesStorage::clear()
{
	alignedOpaque.clear();
	alignedTranslucent.clear();
	nonAlignedOpaque.clear();
	nonAlignedTranslucent.clear();
}

void ChunkMeshFaceStorage::InstancesStorage::swap(InstancesStorage& other) noexcept
{
	alignedOpaque.swap(other.alignedOpaque);
	alignedTranslucent.swap(other.alignedTranslucent);
	nonAlignedOpaque.swap(other.nonAlignedOpaque);
	nonAlignedTranslucent.swap(other.nonAlignedTranslucent);
}
