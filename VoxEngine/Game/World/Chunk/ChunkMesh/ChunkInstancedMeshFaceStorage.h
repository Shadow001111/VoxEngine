#pragma once
#include "MeshTemplates/AlignedBlockFace.h"
#include "MeshTemplates/NonAlignedBlockFace.h"

#include "Core/BlockAllocator.h"
#include "Core/Multithreading/ProcessingFence.h"
#include "Core/Container/DynamicArray.h"

#include <cstdint>

struct ChunkInstancedMeshFaceStorage
{
	struct InstancesStorage
	{
		DynamicArray<AlignedBlockFace> alignedOpaque;
		DynamicArray<AlignedBlockFace> alignedTranslucent;

		DynamicArray<NonAlignedBlockFace> nonAlignedOpaque;
		DynamicArray<NonAlignedBlockFace> nonAlignedTranslucent;

		InstancesStorage& operator=(InstancesStorage&& other) noexcept;
	};

	BlockAllocator::Block allocatedBlock_alignedFaces{};
	BlockAllocator::Block allocatedBlock_nonAlignedFaces{};

	uint16_t renderAlignedOpaqueFaceCount = 0;
	uint16_t renderAlignedTranslucentFaceCount = 0;
	uint32_t renderNonAlignedOpaqueFaceCount = 0;
	uint32_t renderNonAlignedTranslucentFaceCount = 0;

	// TODO: Use flags to reduce memory usage. Though it won't help due to memory layout.
	bool alignedCreated = false;
	bool nonAlignedCreated = false;
	bool shouldBeUploaded = false;

	AtomicWaitFence processingFence;

	InstancesStorage instancesStorage;

	ChunkInstancedMeshFaceStorage() = default;
	~ChunkInstancedMeshFaceStorage() = default;

	void resetRenderFaceCount();
	void updateRenderFaceCount();

	void clearInstances();

	size_t getAlignedOpaqueFaceCount() const { return instancesStorage.alignedOpaque.size(); }
	size_t getAlignedTranslucentFaceCount() const { return instancesStorage.alignedTranslucent.size(); }
	size_t getNonAlignedOpaqueFaceCount() const { return instancesStorage.nonAlignedOpaque.size(); }
	size_t getNonAlignedTranslucentFaceCount() const { return instancesStorage.nonAlignedTranslucent.size(); }

	size_t getAlignedFaceCount() const { return 
		instancesStorage.alignedOpaque.size() +
		instancesStorage.alignedTranslucent.size()
		;};
	size_t getNonAlignedFaceCount() const {
		return
			instancesStorage.nonAlignedOpaque.size() +
			instancesStorage.nonAlignedTranslucent.size()
		;};

	size_t getAllFaceCount() const {
		return
			instancesStorage.alignedOpaque.size() +
			instancesStorage.alignedTranslucent.size() +
			instancesStorage.nonAlignedOpaque.size() +
			instancesStorage.nonAlignedTranslucent.size()
		;};

	size_t getRenderFaceCount() const { return
		(size_t)renderAlignedOpaqueFaceCount +
		(size_t)renderAlignedTranslucentFaceCount +
		(size_t)renderNonAlignedOpaqueFaceCount +
		(size_t)renderNonAlignedTranslucentFaceCount
		;};
	size_t getAlignedFaceCapacity() const { return allocatedBlock_alignedFaces.size; };
	size_t getNonAlignedFaceCapacity() const { return allocatedBlock_nonAlignedFaces.size; };
	size_t getAllFaceCapacity() const {
		return allocatedBlock_alignedFaces.size + allocatedBlock_nonAlignedFaces.size;
	};
};