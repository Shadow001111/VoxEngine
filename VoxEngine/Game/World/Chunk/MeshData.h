#pragma once
#include "BlockFaceInstance.h"

#include "Core/BlockAllocator.h"
#include "Core/Multithreading/ProcessingFence.h"

#include <glad/glad.h>
#include <cstdint>
#include <vector>

struct MeshData
{
	BlockAllocator::Block allocatedBlock;

	uint16_t opaqueFaceCount[6]; // Count for each side
	uint16_t transparentFaceCount[6];

	bool created = false;
	//bool readyToBeRendered = false;
	bool opaqueDirty = false;
	bool transparentDirty = false;

	std::vector<BlockFaceInstance> opaqueInstances;
	std::vector<BlockFaceInstance> transparentInstances;

	ProcessingFence processingFence;

	MeshData();
	~MeshData();

	void resetFaceCount();

	size_t getOpaqueFaceCount() const;
	size_t getTransparentFaceCount() const;
	size_t getFaceCount() const;
	size_t getFaceCapacity() const;
};