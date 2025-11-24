#include "MeshData.h"

ChunkMeshData::ChunkMeshData()
{
}

ChunkMeshData::~ChunkMeshData()
{}

void ChunkMeshData::resetRenderFaceCount()
{
	renderAlignedOpaqueFaceCount = 0;
	renderAlignedTranslucentFaceCount = 0;
	renderNonAlignedOpaqueFaceCount = 0;
	renderNonAlignedTranslucentFaceCount = 0;
}

void ChunkMeshData::updateRenderFaceCount()
{
	renderAlignedOpaqueFaceCount = instancesStorage.alignedOpaque.size();
	renderAlignedTranslucentFaceCount = instancesStorage.alignedTranslucent.size();
	renderNonAlignedOpaqueFaceCount = instancesStorage.nonAlignedOpaque.size();
	renderNonAlignedTranslucentFaceCount = instancesStorage.nonAlignedTranslucent.size();
}

void ChunkMeshData::clearInstances()
{
	instancesStorage.alignedOpaque.clear();
	instancesStorage.alignedTranslucent.clear();
	instancesStorage.nonAlignedOpaque.clear();
	instancesStorage.nonAlignedTranslucent.clear();
}

ChunkMeshData::InstancesStorage& ChunkMeshData::InstancesStorage::operator=(InstancesStorage&& other) noexcept
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
