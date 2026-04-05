#include "Light.h"

LightLevel::LightLevel() :
	fullByte(0)
{}

LightLevel::LightLevel(uint8_t blockLight, uint8_t skyLight) :
	blockLight(blockLight), skyLight(skyLight)
{}

LightLevel::LightLevel(const LightLevel& other) :
	fullByte(other.fullByte)
{}

LightLevel& LightLevel::operator=(const LightLevel& other)
{
	fullByte = other.fullByte;
	return *this;
}


LightPropagationNode::LightPropagationNode() :
	x(0), y(0), z(0)
{
}

LightPropagationNode::LightPropagationNode(uint8_t x, uint8_t y, uint8_t z) :
	x(x), y(y), z(z)
{}


LightRemovalNode::LightRemovalNode() :
	x(0), y(0), z(0), lightLevel(0)
{
}

LightRemovalNode::LightRemovalNode(uint8_t x, uint8_t y, uint8_t z, uint8_t lightLevel) :
	x(x), y(y), z(z), lightLevel(lightLevel)
{}


thread_local ChunkSpecializedQueue<LightPropagationNode> LightPropagationStorage::threadLocalBlockLightPropagation;
thread_local ChunkSpecializedQueue<LightPropagationNode> LightPropagationStorage::threadLocalSkyLightPropagation;
thread_local ChunkSpecializedQueue<LightRemovalNode>	 LightPropagationStorage::threadLocalBlockLightRemoval;
thread_local ChunkSpecializedQueue<LightRemovalNode>	 LightPropagationStorage::threadLocalSkyLightRemoval;

void LightPropagationStorage::LightPropagationQueue::clear()
{
	FenceGuard scopedFence(processingFence);
	queue.clear();
}

void LightPropagationStorage::LightRemovalQueue::clear()
{
	FenceGuard scopedFence(processingFence);
	queue.clear();
}

void LightPropagationStorage::clear()
{
	blockLightPropagation.clear();
	skyLightPropagation.clear();
	blockLightRemoval.clear();
	skyLightRemoval.clear();
}

void LightPropagationStorage::swapQueuesWithLocal()
{
	{
		FenceGuard scopedFence(blockLightPropagation.processingFence);
		blockLightPropagation.queue.swap(threadLocalBlockLightPropagation);
	}
	{
		FenceGuard scopedFence(skyLightPropagation.processingFence);
		skyLightPropagation.queue.swap(threadLocalSkyLightPropagation);
	}
	{
		FenceGuard scopedFence(blockLightRemoval.processingFence);
		blockLightRemoval.queue.swap(threadLocalBlockLightRemoval);
	}
	{
		FenceGuard scopedFence(skyLightRemoval.processingFence);
		skyLightRemoval.queue.swap(threadLocalSkyLightRemoval);
	}
}

void LightPropagationStorage::reserve(size_t count)
{
	blockLightPropagation.queue.reserve(count);
	skyLightPropagation.queue.reserve(count);
	blockLightRemoval.queue.reserve(count);
	skyLightRemoval.queue.reserve(count);
}

void LightPropagationStorage::reserveLocal(size_t count)
{
	threadLocalBlockLightPropagation.reserve(count);
	threadLocalSkyLightPropagation.reserve(count);
	threadLocalBlockLightRemoval.reserve(count);
	threadLocalSkyLightRemoval.reserve(count);
}

bool LightPropagationStorage::hasNodes() const noexcept
{
	return
		blockLightPropagation.queue.size() ||
		skyLightPropagation.queue.size() ||
		blockLightRemoval.queue.size() ||
		skyLightRemoval.queue.size();
}