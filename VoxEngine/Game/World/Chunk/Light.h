#pragma once
#include "ChunkSpecializedQueue.h"

#include "Core/Multithreading/ProcessingFence.h"

union LightLevel
{
	struct
	{
		uint8_t blockLight : 4;
		uint8_t skyLight : 4;
	};

	uint8_t fullByte;

	LightLevel();
	LightLevel(uint8_t blockLight, uint8_t skyLight);

	LightLevel(const LightLevel& other);
	LightLevel& operator=(const LightLevel& other);
};

struct LightPropagationNode
{
	uint8_t x : 4, y : 4, z : 4;

	LightPropagationNode();

	LightPropagationNode(uint8_t x, uint8_t y, uint8_t z);
};

struct LightRemovalNode
{
	uint8_t x : 4, y : 4, z : 4, lightLevel : 4;

	LightRemovalNode();

	LightRemovalNode(uint8_t x, uint8_t y, uint8_t z, uint8_t lightLevel);
};

struct LightPropagationStorage
{
	ChunkSpecializedQueue<LightPropagationNode> blockLightPropagationQueue;
	ChunkSpecializedQueue<LightPropagationNode> skyLightPropagationQueue;
	ChunkSpecializedQueue<LightRemovalNode> blockLightRemovalQueue;
	ChunkSpecializedQueue<LightRemovalNode> skyLightRemovalQueue;

	mutable AtomicWaitFence blockLightPropagationProcessingFence;
	mutable AtomicWaitFence skyLightPropagationProcessingFence;
	mutable AtomicWaitFence blockLightRemovalProcessingFence;
	mutable AtomicWaitFence skyLightRemovalProcessingFence;

	static thread_local ChunkSpecializedQueue<LightPropagationNode>	threadLocalBlockLightPropagation;
	static thread_local ChunkSpecializedQueue<LightPropagationNode>	threadLocalSkyLightPropagation;
	static thread_local	ChunkSpecializedQueue<LightRemovalNode>	threadLocalBlockLightRemoval;
	static thread_local	ChunkSpecializedQueue<LightRemovalNode>	threadLocalSkyLightRemoval;

	void clear();
	void swapQueuesWithLocal();
	void moveQueuesDataToLocal();
	void reserve(size_t count); // Not thread safe

	static void reserveLocal(size_t count);

	bool hasNodes() const noexcept;
	size_t getTotalCapacityInBytes() const noexcept;
};