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

void LightPropagationStorage::clear()
{
	{
		FenceGuard scopedFence(blockLightPropagationProcessingFence);
		blockLightPropagationQueue.clear();
	}
	{
		FenceGuard scopedFence(skyLightPropagationProcessingFence);
		skyLightPropagationQueue.clear();
	}
	{
		FenceGuard scopedFence(blockLightRemovalProcessingFence);
		blockLightRemovalQueue.clear();
	}
	{
		FenceGuard scopedFence(skyLightRemovalProcessingFence);
		skyLightRemovalQueue.clear();
	}
}

void LightPropagationStorage::swapQueuesWithLocal()
{
	{
		FenceGuard scopedFence(blockLightPropagationProcessingFence);
		blockLightPropagationQueue.swap(threadLocalBlockLightPropagation);
	}
	{
		FenceGuard scopedFence(skyLightPropagationProcessingFence);
		skyLightPropagationQueue.swap(threadLocalSkyLightPropagation);
	}
	{
		FenceGuard scopedFence(blockLightRemovalProcessingFence);
		blockLightRemovalQueue.swap(threadLocalBlockLightRemoval);
	}
	{
		FenceGuard scopedFence(skyLightRemovalProcessingFence);
		skyLightRemovalQueue.swap(threadLocalSkyLightRemoval);
	}
}

void LightPropagationStorage::reserve(size_t count)
{
	blockLightPropagationQueue.reserve(count);
	skyLightPropagationQueue.reserve(count);
	blockLightRemovalQueue.reserve(count);
	skyLightRemovalQueue.reserve(count);
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
		blockLightPropagationQueue.size() ||
		skyLightPropagationQueue.size() ||
		blockLightRemovalQueue.size() ||
		skyLightRemovalQueue.size();
}