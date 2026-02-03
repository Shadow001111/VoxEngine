#include "MeshData.h"

void ChunkMeshData::resetRenderFaceCount()
{
	renderAlignedOpaqueFaceCount.fill(0);
	renderAlignedTranslucentFaceCount.fill(0);
	renderNonAlignedOpaqueFaceCount = 0;
	renderNonAlignedTranslucentFaceCount = 0;
}

void ChunkMeshData::updateRenderFaceCount()
{
	for (size_t i = 0; i < 6; i++)
	{
		renderAlignedOpaqueFaceCount[i] = instancesStorage.alignedOpaque[i].size();
		renderAlignedTranslucentFaceCount[i] = instancesStorage.alignedTranslucent[i].size();
	}
	renderNonAlignedOpaqueFaceCount = instancesStorage.nonAlignedOpaque.size();
	renderNonAlignedTranslucentFaceCount = instancesStorage.nonAlignedTranslucent.size();
}

void ChunkMeshData::clearInstances()
{
	for (auto& vec : instancesStorage.alignedOpaque)
	{
		vec.clear();
	}
	for (auto& vec : instancesStorage.alignedTranslucent)
	{
		vec.clear();
	}
	instancesStorage.nonAlignedOpaque.clear();
	instancesStorage.nonAlignedTranslucent.clear();
}

size_t ChunkMeshData::getAlignedOpaqueFaceCount() const
{
	size_t count = 0;
	for (const auto& vec : instancesStorage.alignedOpaque)
	{
		count += vec.size();
	}
	return count;
}

size_t ChunkMeshData::getAlignedTranslucentFaceCount() const
{
	size_t count = 0;
	for (const auto& vec : instancesStorage.alignedTranslucent)
	{
		count += vec.size();
	}
	return count;
}

size_t ChunkMeshData::getRenderFaceCount() const
{
	size_t count = 0;
	for (const auto& cnt : renderAlignedOpaqueFaceCount)
	{
		count += cnt;
	}
	for (const auto& cnt : renderAlignedTranslucentFaceCount)
	{
		count += cnt;
	}
	count += renderNonAlignedOpaqueFaceCount;
	count += renderNonAlignedTranslucentFaceCount;
	return count;
}

ChunkMeshData::InstancesStorage& ChunkMeshData::InstancesStorage::operator=(InstancesStorage&& other) noexcept
{
	if (this != &other)
	{
		for (size_t i = 0; i < 6; i++)
		{
			alignedOpaque[i] = std::move(other.alignedOpaque[i]);
			alignedTranslucent[i] = std::move(other.alignedTranslucent[i]);
		}

		nonAlignedOpaque = std::move(other.nonAlignedOpaque);
		nonAlignedTranslucent = std::move(other.nonAlignedTranslucent);
	}
	return *this;
}
