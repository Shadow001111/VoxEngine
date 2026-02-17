#include "ChunkRegion.h"

glm::ivec3 ChunkRegion::getRegionPosition(const glm::ivec3& chunkPosition)
{
	return chunkPosition >> CHUNK_REGION_SIZE_LOG2;
}

size_t ChunkRegion::getChunkIndexInRegion(const glm::ivec3& chunkPosition)
{
	glm::ivec3 chunkPosInRegion = chunkPosition & CHUNK_REGION_LOWER_BITS_MASK;
	return (chunkPosInRegion.x << (CHUNK_REGION_SIZE_LOG2 << 1)) | (chunkPosInRegion.y << CHUNK_REGION_SIZE_LOG2) | chunkPosInRegion.z;
}

void ChunkRegion::init()
{
	chunks.fill(nullptr);
	chunkCount = 0;
}