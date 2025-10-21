#pragma once
#include "BlockFaceInstance.h"

#include "Core/BlockAllocator.h"
#include "Core/Multithreading/ProcessingFence.h"

#include <cstdint>
#include <vector>

struct MeshData
{
	BlockAllocator::Block allocatedBlock;

	uint16_t opaqueFaceCount = 0;
	uint16_t transparentFaceCount = 0;

	bool created = false;
	bool opaqueDirty = false;
	bool transparentDirty = false;

	std::vector<BlockFaceInstance> opaqueInstances;
	std::vector<BlockFaceInstance> transparentInstances;

	ProcessingFence processingFence;

	MeshData();
	~MeshData();

	void resetFaceCount();

	size_t getFaceCount() const { return opaqueFaceCount + transparentFaceCount; };
	size_t getFaceCapacity() const { return allocatedBlock.size; };
};