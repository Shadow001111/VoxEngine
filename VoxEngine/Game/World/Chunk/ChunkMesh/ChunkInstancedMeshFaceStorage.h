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

	BlockAllocator<uint32_t>::Block allocatedBlock_alignedFaces{};
	BlockAllocator<uint32_t>::Block allocatedBlock_nonAlignedFaces{};

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

	uint32_t getAlignedOpaqueFaceCount() const { return static_cast<uint32_t>(instancesStorage.alignedOpaque.size()); }
	uint32_t getAlignedTranslucentFaceCount() const { return static_cast<uint32_t>(instancesStorage.alignedTranslucent.size()); }
	uint32_t getNonAlignedOpaqueFaceCount() const { return static_cast<uint32_t>(instancesStorage.nonAlignedOpaque.size()); }
	uint32_t getNonAlignedTranslucentFaceCount() const { return static_cast<uint32_t>(instancesStorage.nonAlignedTranslucent.size()); }

	uint32_t getAlignedFaceCount() const {
		return
			static_cast<uint32_t>(instancesStorage.alignedOpaque.size()) +
			static_cast<uint32_t>(instancesStorage.alignedTranslucent.size())
			;
	};
	uint32_t getNonAlignedFaceCount() const {
		return
			static_cast<uint32_t>(instancesStorage.nonAlignedOpaque.size()) +
			static_cast<uint32_t>(instancesStorage.nonAlignedTranslucent.size())
			;
	};

	uint32_t getAllFaceCount() const {
		return
			static_cast<uint32_t>(instancesStorage.alignedOpaque.size()) +
			static_cast<uint32_t>(instancesStorage.alignedTranslucent.size()) +
			static_cast<uint32_t>(instancesStorage.nonAlignedOpaque.size()) +
			static_cast<uint32_t>(instancesStorage.nonAlignedTranslucent.size())
			;
	};

	uint32_t getRenderFaceCount() const {
		return
			(uint32_t)renderAlignedOpaqueFaceCount +
			(uint32_t)renderAlignedTranslucentFaceCount +
			(uint32_t)renderNonAlignedOpaqueFaceCount +
			(uint32_t)renderNonAlignedTranslucentFaceCount
			;
	};
	uint32_t getAlignedFaceCapacity() const { return static_cast<uint32_t>(allocatedBlock_alignedFaces.size); };
	uint32_t getNonAlignedFaceCapacity() const { return static_cast<uint32_t>(allocatedBlock_nonAlignedFaces.size); };
	uint32_t getAllFaceCapacity() const {
		return static_cast<uint32_t>(allocatedBlock_alignedFaces.size) + static_cast<uint32_t>(allocatedBlock_nonAlignedFaces.size);
	};
};