#pragma once
#include "MeshTemplates/AlignedBlockFace.h"
#include "MeshTemplates/NonAlignedBlockFace.h"

#include "Core/BlockAllocator.h"
#include "Core/Multithreading/ProcessingFence.h"

#include <cstdint>
#include <vector>
#include <array>

struct ChunkMeshData
{
	// TODO: Keep aligned faces in single container, not 6 separate ones.
	// TODO: Use lighter vectors, no storing allocator, custom index type.
	struct InstancesStorage
	{
		std::array<std::vector<AlignedBlockFace>, 6> alignedOpaque;
		std::array<std::vector<AlignedBlockFace>, 6> alignedTranslucent;

		std::vector<NonAlignedBlockFace> nonAlignedOpaque;
		std::vector<NonAlignedBlockFace> nonAlignedTranslucent;

		InstancesStorage& operator=(InstancesStorage&& other) noexcept;
	};

	BlockAllocator::Block allocatedBlock_alignedFaces{};
	BlockAllocator::Block allocatedBlock_nonAlignedFaces{};

	std::array<uint16_t, 6> renderAlignedOpaqueFaceCount = { 0, 0, 0, 0, 0, 0 };
	std::array<uint16_t, 6> renderAlignedTranslucentFaceCount = { 0, 0, 0, 0, 0, 0 };
	uint32_t renderNonAlignedOpaqueFaceCount = 0;
	uint32_t renderNonAlignedTranslucentFaceCount = 0;

	// TODO: Use flags to reduce memory usage. Though it won't help due to memory layout.
	bool alignedCreated = false;
	bool nonAlignedCreated = false;
	bool dirty = false;

	InstancesStorage instancesStorage;

	ProcessingFence processingFence;

	void resetRenderFaceCount();
	void updateRenderFaceCount();

	void clearInstances();

	size_t getAlignedOpaqueFaceCount() const;
	size_t getAlignedTranslucentFaceCount() const;
	size_t getNonAlignedOpaqueFaceCount() const { return instancesStorage.nonAlignedOpaque.size(); }
	size_t getNonAlignedTranslucentFaceCount() const { return instancesStorage.nonAlignedTranslucent.size(); }

	size_t getAlignedFaceCount() const { return getAlignedOpaqueFaceCount() + getAlignedTranslucentFaceCount(); };
	size_t getNonAlignedFaceCount() const { return instancesStorage.nonAlignedOpaque.size() + instancesStorage.nonAlignedTranslucent.size(); };

	size_t getAllFaceCount() const { return getAlignedFaceCount() + getNonAlignedFaceCount() ;};

	size_t getRenderFaceCount() const;
	size_t getAlignedFaceCapacity() const { return allocatedBlock_alignedFaces.size; };
	size_t getNonAlignedFaceCapacity() const { return allocatedBlock_nonAlignedFaces.size; };
	size_t getAllFaceCapacity() const { return allocatedBlock_alignedFaces.size + allocatedBlock_nonAlignedFaces.size; };
};