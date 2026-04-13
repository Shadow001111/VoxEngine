#include "ChunkMeshFaceStorage.h"

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

void ChunkMeshFaceStorage::InstancesStorage::shrinkToFit()
{
	alignedOpaque.shrink_to_fit();
	alignedTranslucent.shrink_to_fit();
	unalignedOpaque.shrink_to_fit();
	unalignedTranslucent.shrink_to_fit();
}

const void* ChunkMeshFaceStorage::InstancesStorage::data(MeshLayer layer) const noexcept
{
	switch (layer)
	{
	case MeshLayer::AlignedOpaque:        return alignedOpaque.data();
	case MeshLayer::AlignedTranslucent:   return alignedTranslucent.data();
	case MeshLayer::UnalignedOpaque:      return unalignedOpaque.data();
	case MeshLayer::UnalignedTranslucent: return unalignedTranslucent.data();
	default: return nullptr;
	}
}

uint32_t ChunkMeshFaceStorage::InstancesStorage::size(MeshLayer layer) const noexcept
{
	switch (layer)
	{
	case MeshLayer::AlignedOpaque:      return static_cast<uint32_t>(alignedOpaque.size());
	case MeshLayer::AlignedTranslucent: return static_cast<uint32_t>(alignedTranslucent.size());
	case MeshLayer::UnalignedOpaque:    return static_cast<uint32_t>(unalignedOpaque.size());
	case MeshLayer::UnalignedTranslucent: return static_cast<uint32_t>(unalignedTranslucent.size());
	default: return 0;
	}
}

void ChunkMeshFaceStorage::updateRenderFaceCount()
{
	for (int i = 0; i < (int)MeshLayer::Count; i++)
	{
		renderFaceCounts[i] = instancesStorage.size(static_cast<MeshLayer>(i));
	}
}

uint32_t ChunkMeshFaceStorage::getAllFaceCount() const noexcept
{
	uint32_t total = 0;
	for (int i = 0; i < (int)MeshLayer::Count; i++)
	{
		total += getFaceCount(static_cast<MeshLayer>(i));
	}
	return total;
}

uint32_t ChunkMeshFaceStorage::getAllFaceCapacity() const noexcept
{
	uint32_t total = 0;
	for (const auto& block : facesBlocks)
		total += block.size;
	return total;
}

uint32_t ChunkMeshFaceStorage::getAllRenderFaceCount() const noexcept
{
	uint32_t total = 0;
	for (uint32_t c : renderFaceCounts)
		total += c;
	return total;
}
