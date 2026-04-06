#pragma once
#include "MeshTemplates/AlignedBlockFace.h"
#include "MeshTemplates/UnalignedBlockFace.h"

#include "Core/BlockAllocator.h"
#include "Core/Multithreading/ProcessingFence.h"
#include "Core/Container/DynamicArray.h"

#include <cstdint>

struct ChunkMeshFaceStorage
{
	struct InstancesStorage
	{
		DynamicArray<AlignedBlockFace> alignedOpaque;
		DynamicArray<AlignedBlockFace> alignedTranslucent;

		DynamicArray<UnalignedBlockFace> unalignedOpaque;
		DynamicArray<UnalignedBlockFace> unalignedTranslucent;

		InstancesStorage& operator=(InstancesStorage&& other) noexcept;

		void clear();
		void swap(InstancesStorage& other) noexcept;
	};

	BlockAllocator<uint32_t>::Block allocatedBlock_alignedFaces{};
	BlockAllocator<uint32_t>::Block allocatedBlock_unalignedFaces{};

	uint16_t renderAlignedOpaqueFaceCount = 0;
	uint16_t renderAlignedTranslucentFaceCount = 0;
	uint32_t renderUnalignedOpaqueFaceCount = 0;
	uint32_t renderUnalignedTranslucentFaceCount = 0;

	// TODO: Use flags to reduce memory usage. Though it won't help due to memory layout.
	bool alignedCreated = false;
	bool unalignedCreated = false;
	bool shouldBeUploaded = false;

	AtomicWaitFence processingFence;

	InstancesStorage instancesStorage;

	ChunkMeshFaceStorage() = default;
	~ChunkMeshFaceStorage() = default;

	void resetRenderFaceCount();
	void updateRenderFaceCount();

	void clearInstances() { instancesStorage.clear(); }

	uint32_t getAlignedOpaqueFaceCount() const noexcept { return static_cast<uint32_t>(instancesStorage.alignedOpaque.size()); }
	uint32_t getAlignedTranslucentFaceCount() const noexcept { return static_cast<uint32_t>(instancesStorage.alignedTranslucent.size()); }
	uint32_t getUnalignedOpaqueFaceCount() const noexcept { return static_cast<uint32_t>(instancesStorage.unalignedOpaque.size()); }
	uint32_t getUnalignedTranslucentFaceCount() const noexcept { return static_cast<uint32_t>(instancesStorage.unalignedTranslucent.size()); }

	uint32_t getAlignedFaceCount() const noexcept {
		return
			static_cast<uint32_t>(instancesStorage.alignedOpaque.size()) +
			static_cast<uint32_t>(instancesStorage.alignedTranslucent.size())
			;
	};
	uint32_t getUnalignedFaceCount() const noexcept {
		return
			static_cast<uint32_t>(instancesStorage.unalignedOpaque.size()) +
			static_cast<uint32_t>(instancesStorage.unalignedTranslucent.size())
			;
	};

	uint32_t getAllFaceCount() const noexcept {
		return
			static_cast<uint32_t>(instancesStorage.alignedOpaque.size()) +
			static_cast<uint32_t>(instancesStorage.alignedTranslucent.size()) +
			static_cast<uint32_t>(instancesStorage.unalignedOpaque.size()) +
			static_cast<uint32_t>(instancesStorage.unalignedTranslucent.size())
			;
	};

	uint32_t getRenderFaceCount() const noexcept {
		return
			(uint32_t)renderAlignedOpaqueFaceCount +
			(uint32_t)renderAlignedTranslucentFaceCount +
			(uint32_t)renderUnalignedOpaqueFaceCount +
			(uint32_t)renderUnalignedTranslucentFaceCount
			;
	};
	uint32_t getAlignedFaceCapacity() const noexcept { return static_cast<uint32_t>(allocatedBlock_alignedFaces.size); };
	uint32_t getUnalignedFaceCapacity() const noexcept { return static_cast<uint32_t>(allocatedBlock_unalignedFaces.size); };
	uint32_t getAllFaceCapacity() const noexcept {
		return static_cast<uint32_t>(allocatedBlock_alignedFaces.size) + static_cast<uint32_t>(allocatedBlock_unalignedFaces.size);
	};
};