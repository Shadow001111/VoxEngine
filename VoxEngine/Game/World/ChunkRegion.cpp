#include "ChunkRegion.h"
#include "Chunk/ChunkIO.h"

#include "Core/Multithreading/ThreadPool.h"

AtomicFlags<uint8_t> ChunkRegion::globalFlags;

size_t ChunkRegion::getChunkIndexInRegion(const glm::ivec3& chunkPosition) noexcept
{
	glm::ivec3 chunkPosInRegion = chunkPosition & CHUNK_REGION_LOWER_BITS_MASK;
	return (chunkPosInRegion.x << (CHUNK_REGION_SIZE_LOG2 << 1)) | (chunkPosInRegion.y << CHUNK_REGION_SIZE_LOG2) | chunkPosInRegion.z;
}

void ChunkRegion::init(const glm::ivec3& regionPosition)
{
	position = regionPosition;

	chunks.fill(nullptr);

	chunkCount = 0;
	renderChunkCount = 0;

	flags.reset();

	ThreadPool& threadPool = ParallelUtils::getGlobalThreadPool();

	threadPool.enqueue([this, pos = position]()
		{
			savedChunksMask = ChunkIO::checkChunkRegionForSaves(pos);
			isSavedChunksMaskInitialized.store(true, std::memory_order_release);
		});
}