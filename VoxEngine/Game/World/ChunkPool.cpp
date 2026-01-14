#include "ChunkPool.h"

void ChunkPool::release(Chunk* chunk)
{
	chunk->destroy();

	if (chunk->getIsProcessing())
	{
		processingChunks.push_back(chunk);
	}
	else
	{
		pool.push_back(chunk);
	}
}

void ChunkPool::returnProcessingChunksToPool()
{
	size_t count = processingChunks.size();
	if (count == 0)
	{
		return;
	}

	auto it = processingChunks.begin();
	while (it != processingChunks.end())
	{
		Chunk* chunk = *it;

		if (!chunk->getIsProcessing())
		{
			pool.push_back(chunk);
			it = processingChunks.erase(it);
		}
		else
		{
			++it;
		}
	}
}