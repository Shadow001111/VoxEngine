#pragma once
#include "Chunk.h"

#include <vector>
#include <memory>

class ChunkPool
{
	std::vector<std::unique_ptr<Chunk>> pool;
	std::vector<std::unique_ptr<Chunk>> processingChunks;
public:
	ChunkPool() = default;
	~ChunkPool() = default;

	ChunkPool(const ChunkPool&) = delete;
	ChunkPool& operator=(const ChunkPool&) = delete;
	ChunkPool(ChunkPool&&) = delete;
	ChunkPool& operator=(ChunkPool&&) = delete;

	std::unique_ptr<Chunk> acquire();
	void release(std::unique_ptr<Chunk> chunk);

	// Allocates only if 'count' is bigger than pool size
	void allocate(size_t count);

	void returnProcessingChunksToPool();
};

