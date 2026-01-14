#pragma once
#include "Chunk.h"
#include "Core/MemoryAllocation/FixedArenaObjectPool.h"

class ChunkPool : public FixedArenaObjectPool<Chunk>
{
	std::vector<Chunk*> processingChunks;
public:
	ChunkPool() = default;
	~ChunkPool() = default;

	ChunkPool(const ChunkPool&) = delete;
	ChunkPool& operator=(const ChunkPool&) = delete;
	ChunkPool(ChunkPool&&) = delete;
	ChunkPool& operator=(ChunkPool&&) = delete;

	void release(Chunk* chunk);

	void returnProcessingChunksToPool();
};

