#pragma once
#include "MeshTemplates/AlignedBlockFace.h"
#include "MeshTemplates/UnalignedBlockFace.h"

#include "Core/Flags.h"
#include "Core/BlockAllocator.h"
#include "Core/Multithreading/ProcessingFence.h"
#include "Core/Container/DynamicArray.h"

#include <cstdint>
#include <array>

enum class MeshLayer : uint8_t
{
	AlignedOpaque = 0,
	AlignedTranslucent = 1,
	UnalignedOpaque = 2,
	UnalignedTranslucent = 3,
	Count = 4
};

constexpr bool isAligned(MeshLayer layer) noexcept
{
	return layer == MeshLayer::AlignedOpaque || layer == MeshLayer::AlignedTranslucent;
}

constexpr size_t faceStructSize(MeshLayer layer) noexcept
{
	return isAligned(layer) ? sizeof(AlignedBlockFace) : sizeof(UnalignedBlockFace);
}

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

		const void* data(MeshLayer layer) const noexcept;
		uint32_t size(MeshLayer layer) const noexcept;
	};

	enum class Flag : uint8_t
	{
		ShouldBeUploaded = 0,

		// These four must stay contiguous – createdFlag() relies on it.
		AlignedOpaqueCreated = 1,
		AlignedTranslucentCreated = 2,
		UnalignedOpaqueCreated = 3,
		UnalignedTranslucentCreated = 4
	};

	using Block = BlockAllocator<uint32_t>::Block;

	std::array<Block, 4> facesBlocks{};

	std::array<uint32_t, 4> renderFaceCounts{};

	Flags<uint8_t> flags;

	AtomicWaitFence processingFence;

	InstancesStorage instancesStorage;
public:
	static constexpr Flag createdFlag(MeshLayer layer) noexcept
	{
		return static_cast<Flag>(
			static_cast<uint8_t>(Flag::AlignedOpaqueCreated) +
			static_cast<uint8_t>(layer)
			);
	}

	ChunkMeshFaceStorage() = default;
	~ChunkMeshFaceStorage() = default;

	void resetRenderFaceCount() noexcept { renderFaceCounts.fill(0); }
	void updateRenderFaceCount();

	void clearInstances() { instancesStorage.clear(); }

	// Flag helpers

	void setFlag(Flag flag, bool value) noexcept { flags.set(static_cast<unsigned>(flag), value); }
	bool readFlag(Flag flag) const noexcept { return flags.read(static_cast<unsigned>(flag)); }
	bool readAndSetFlag(Flag flag, bool value) noexcept { return flags.readAndSet(static_cast<unsigned>(flag), value); }

	// Per-layer accessors

	const void* getFaceData(MeshLayer layer) const noexcept { return instancesStorage.data(layer); }
	uint32_t getFaceCount(MeshLayer layer) const noexcept { return instancesStorage.size(layer); }
	uint32_t getFaceCapacity(MeshLayer layer) const noexcept { return facesBlocks[static_cast<uint8_t>(layer)].size; }
	BlockAllocator<uint32_t>::Block& getFacesBlock(MeshLayer layer) noexcept { return facesBlocks[static_cast<uint8_t>(layer)]; }
	// Sum accessors

	uint32_t getAllFaceCount() const noexcept;
	uint32_t getAllFaceCapacity() const noexcept;
	uint32_t getAllRenderFaceCount() const noexcept;
};