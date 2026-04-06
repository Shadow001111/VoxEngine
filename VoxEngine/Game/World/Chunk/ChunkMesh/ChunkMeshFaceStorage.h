#pragma once
#include "MeshTemplates/AlignedBlockFace.h"
#include "MeshTemplates/UnalignedBlockFace.h"

#include "Core/Flags.h"
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

	enum class Flag : uint8_t
	{
		ShouldBeUploaded,

		AlignedOpaqueCreated,
		AlignedTranslucentCreated,
		UnalignedOpaqueCreated,
		UnalignedTranslucentCreated
	};

	BlockAllocator<uint32_t>::Block alignedOpaqueFacesBlock{};
	BlockAllocator<uint32_t>::Block alignedTranslucentFacesBlock{};
	BlockAllocator<uint32_t>::Block unalignedOpaqueFacesBlock{};
	BlockAllocator<uint32_t>::Block unalignedTranslucentFacesBlock{};

	uint16_t renderAlignedOpaqueFaceCount = 0;
	uint16_t renderAlignedTranslucentFaceCount = 0;
	uint32_t renderUnalignedOpaqueFaceCount = 0;
	uint32_t renderUnalignedTranslucentFaceCount = 0;

	Flags<uint8_t> flags;

	AtomicWaitFence processingFence;

	InstancesStorage instancesStorage;

	ChunkMeshFaceStorage() = default;
	~ChunkMeshFaceStorage() = default;

	void resetRenderFaceCount();
	void updateRenderFaceCount();

	void clearInstances() { instancesStorage.clear(); }

	void setFlag(Flag flag, bool value) noexcept { flags.set(static_cast<unsigned>(flag), value); }
	bool readFlag(Flag flag) const noexcept { return flags.read(static_cast<unsigned>(flag)); }
	bool readAndSetFlag(Flag flag, bool value) noexcept { return flags.readAndSet(static_cast<unsigned>(flag), value); }

	uint32_t getAlignedOpaqueFaceCount() const noexcept { return instancesStorage.alignedOpaque.size(); }
	uint32_t getAlignedTranslucentFaceCount() const noexcept { return instancesStorage.alignedTranslucent.size(); }
	uint32_t getUnalignedOpaqueFaceCount() const noexcept { return instancesStorage.unalignedOpaque.size(); }
	uint32_t getUnalignedTranslucentFaceCount() const noexcept { return instancesStorage.unalignedTranslucent.size(); }

	uint32_t getAlignedOpaqueFaceCapacity() const noexcept { return alignedOpaqueFacesBlock.size; }
	uint32_t getAlignedTranslucentFaceCapacity() const noexcept { return alignedTranslucentFacesBlock.size; }
	uint32_t getUnalignedOpaqueFaceCapacity() const noexcept { return unalignedOpaqueFacesBlock.size; }
	uint32_t getUnalignedTranslucentFaceCapacity() const noexcept { return unalignedTranslucentFacesBlock.size; }

	uint32_t getAllFaceCount() const noexcept {
		return
			getAlignedOpaqueFaceCount() +
			getAlignedTranslucentFaceCount() +
			getUnalignedOpaqueFaceCount() +
			getUnalignedTranslucentFaceCount()
			;
	};

	uint32_t getAllFaceCapacity() const noexcept {
		return
			getAlignedOpaqueFaceCapacity() +
			getAlignedTranslucentFaceCapacity() +
			getUnalignedOpaqueFaceCapacity() +
			getUnalignedTranslucentFaceCapacity()
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
};