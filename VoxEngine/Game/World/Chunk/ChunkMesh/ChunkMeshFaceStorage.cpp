#include "ChunkMeshFaceStorage.h"

void ChunkMeshFaceStorage::resetRenderFaceCount()
{
	renderAlignedOpaqueFaceCount = 0;
	renderAlignedTranslucentFaceCount = 0;
	renderUnalignedOpaqueFaceCount = 0;
	renderUnalignedTranslucentFaceCount = 0;
}

void ChunkMeshFaceStorage::updateRenderFaceCount()
{
	renderAlignedOpaqueFaceCount = instancesStorage.alignedOpaque.size();
	renderAlignedTranslucentFaceCount = instancesStorage.alignedTranslucent.size();
	renderUnalignedOpaqueFaceCount = instancesStorage.unalignedOpaque.size();
	renderUnalignedTranslucentFaceCount = instancesStorage.unalignedTranslucent.size();
}

ChunkMeshFaceStorage::InstancesStorage& ChunkMeshFaceStorage::InstancesStorage::operator=(InstancesStorage&& other) noexcept
{
	if (this != &other)
	{
		alignedOpaque = std::move(other.alignedOpaque);
		alignedTranslucent = std::move(other.alignedTranslucent);

		unalignedOpaque = std::move(other.unalignedOpaque);
		unalignedTranslucent = std::move(other.unalignedTranslucent);
	}
	return *this;
}



void ChunkMeshFaceStorage::InstancesStorage::clear()
{
	alignedOpaque.clear();
	alignedTranslucent.clear();
	unalignedOpaque.clear();
	unalignedTranslucent.clear();
}

void ChunkMeshFaceStorage::InstancesStorage::swap(InstancesStorage& other) noexcept
{
	alignedOpaque.swap(other.alignedOpaque);
	alignedTranslucent.swap(other.alignedTranslucent);
	unalignedOpaque.swap(other.unalignedOpaque);
	unalignedTranslucent.swap(other.unalignedTranslucent);
}
