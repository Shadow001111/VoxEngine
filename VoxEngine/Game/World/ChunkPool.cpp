#include "ChunkPool.h"

#include <cassert>

std::unique_ptr<Chunk> ChunkPool::acquire()
{
	if (!pool.empty())
	{
		std::unique_ptr<Chunk> chunk = std::move(pool.back());
		pool.pop_back();
		return chunk;
	}

	{
		size_t allocateCount = 10;
		pool.reserve(pool.size() + allocateCount);
		for (size_t i = 0; i < allocateCount; i++)
		{
			pool.push_back(std::make_unique<Chunk>());
		}
	}

	return std::make_unique<Chunk>();
}

void ChunkPool::release(std::unique_ptr<Chunk> chunk)
{
	assert(chunk->getIsLoadedInWorld());

	chunk->destroy();

	if (chunk->getIsProcessing())
	{
		processingChunks.push_back(std::move(chunk));
	}
	else
	{
		pool.push_back(std::move(chunk));
	}
}

void ChunkPool::allocate(size_t count)
{
	size_t poolSize = pool.size();
	pool.reserve(poolSize + count);
	for (size_t i = poolSize; i < count; i++)
	{
		pool.push_back(std::make_unique<Chunk>());
	}
}

void ChunkPool::returnProcessingChunksToPool()
{
	size_t count = processingChunks.size();
	if (count == 0)
	{
		return;
	}

	pool.reserve(pool.size() + count);

	auto it = processingChunks.begin();
	while (it != processingChunks.end())
	{
		std::unique_ptr<Chunk>& chunk = *it;

		if (!chunk->getIsProcessing())
		{
			pool.push_back(std::move(chunk));
			it = processingChunks.erase(it);
		}
		else
		{
			++it;
		}
	}
}