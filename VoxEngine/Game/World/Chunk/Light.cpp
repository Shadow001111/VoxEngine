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


// TODO: Try allocate these queues only for VoxEngine threads
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

void LightPropagationStorage::moveQueuesDataToLocalAndShrink()
{
	{
		FenceGuard scopedFence(blockLightPropagationProcessingFence);
		blockLightPropagationQueue.move_data_to(threadLocalBlockLightPropagation);
		blockLightPropagationQueue.shrink_to_default();
	}
	{
		FenceGuard scopedFence(skyLightPropagationProcessingFence);
		skyLightPropagationQueue.move_data_to(threadLocalSkyLightPropagation);
		skyLightPropagationQueue.shrink_to_default();
	}
	{
		FenceGuard scopedFence(blockLightRemovalProcessingFence);
		blockLightRemovalQueue.move_data_to(threadLocalBlockLightRemoval);
		blockLightRemovalQueue.shrink_to_default();
	}
	{
		FenceGuard scopedFence(skyLightRemovalProcessingFence);
		skyLightRemovalQueue.move_data_to(threadLocalSkyLightRemoval);
		skyLightRemovalQueue.shrink_to_default();
	}
}

void LightPropagationStorage::reserve(size_t count)
{
	{
		FenceGuard scopedFence(blockLightPropagationProcessingFence);
		blockLightPropagationQueue.reserve(count);
	}
	{
		FenceGuard scopedFence(skyLightPropagationProcessingFence);
		skyLightPropagationQueue.reserve(count);
	}
	{
		FenceGuard scopedFence(blockLightRemovalProcessingFence);
		blockLightRemovalQueue.reserve(count);
	}
	{
		FenceGuard scopedFence(skyLightRemovalProcessingFence);
		skyLightRemovalQueue.reserve(count);
	}
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
	{
		FenceGuard scopedFence(blockLightPropagationProcessingFence);
		if (!blockLightPropagationQueue.empty()) return true;
	}
	{
		FenceGuard scopedFence(skyLightPropagationProcessingFence);
		if (!skyLightPropagationQueue.empty()) return true;
	}
	{
		FenceGuard scopedFence(blockLightRemovalProcessingFence);
		if (!blockLightRemovalQueue.empty()) return true;
	}
	{
		FenceGuard scopedFence(skyLightRemovalProcessingFence);
		if (!skyLightRemovalQueue.empty()) return true;
	}
	return false;
}

size_t LightPropagationStorage::getTotalCapacityInBytes() const noexcept
{
	size_t capacity = 0;
	{
		FenceGuard scopedFence(blockLightPropagationProcessingFence);
		capacity += blockLightPropagationQueue.capacity();
	}
	{
		FenceGuard scopedFence(skyLightPropagationProcessingFence);
		capacity += skyLightPropagationQueue.capacity();
	}
	{
		FenceGuard scopedFence(blockLightRemovalProcessingFence);
		capacity += blockLightRemovalQueue.capacity();
	}
	{
		FenceGuard scopedFence(skyLightRemovalProcessingFence);
		capacity += skyLightRemovalQueue.capacity();
	}
	static_assert(sizeof(LightPropagationNode) == sizeof(LightRemovalNode), "Must be the same for correct math");
	return capacity * sizeof(LightPropagationNode);
}
