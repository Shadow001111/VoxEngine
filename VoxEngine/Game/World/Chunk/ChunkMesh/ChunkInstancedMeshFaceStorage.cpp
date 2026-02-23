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

void ChunkInstancedMeshFaceStorage::clearInstances()
{
	instancesStorage.alignedOpaque.clear();
	instancesStorage.alignedTranslucent.clear();
	instancesStorage.nonAlignedOpaque.clear();
	instancesStorage.nonAlignedTranslucent.clear();
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
