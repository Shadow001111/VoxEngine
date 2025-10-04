#include "World.h"

#include "Profiler.h"
#include "ThreadPool.h"

#include <iostream>

World::World()
{
}

World::~World()
{
}

void World::loadChunksAroundPlayer(const Int3& chunkLoaderPos, int renderDistance)
{
	if (!firstLoad && lastChunkLoaderPos == chunkLoaderPos)
    {
        return;
    }
	firstLoad = false;
	lastChunkLoaderPos = chunkLoaderPos;

	// Unload chunks that are out of range
	{
		PROFILE_SCOPE("Unload chunks");

		std::vector<Int3> chunksToUnload;
		for (const auto& pair : chunks)
		{
			const Int3& pos = pair.first;
			if (std::abs(pos.x - chunkLoaderPos.x) > renderDistance ||
				std::abs(pos.y - chunkLoaderPos.y) > renderDistance ||
				std::abs(pos.z - chunkLoaderPos.z) > renderDistance)
			{
				chunksToUnload.push_back(pos);
			}
		}
		for (const Int3& pos : chunksToUnload)
		{
			// Return chunk to pool
			chunkPool.release(std::move(chunks[pos]));

			// Remove from map
			chunks.erase(pos);
		}
	}

	// Load chunks in a cubic area around the chunkLoaderPos
	// TODO: Make area spherical
	{
		PROFILE_SCOPE("Load chunks");

		for (int x = -renderDistance; x <= renderDistance; x++)
		{
			int chunkX = chunkLoaderPos.x + x;
			for (int y = -renderDistance; y <= renderDistance; y++)
			{
				int chunkY = chunkLoaderPos.y + y;
				for (int z = -renderDistance; z <= renderDistance; z++)
				{
					int chunkZ = chunkLoaderPos.z + z;
					loadChunk(chunkX, chunkY, chunkZ);
				}
			}
		}
	}
}

void World::update()
{
	if (!blocksBuildChunkContainer.empty())
	{
		buildChunkBlocks();
	}

	if (!meshBuildChunkContainer.empty())
	{
		buildChunkMeshes();
	}
}

void World::render(const Shader& faceShader) const
{
	// TODO: Use ssbo for chunk's position. Maybe it's faster? Though takes much more memory.
	for (const auto& pair : chunks)
	{
		const Chunk* chunk = pair.second.get();

		Int3 pos = chunk->getPosition();
		glm::vec3 chunkWorldPos = glm::vec3(pos.x, pos.y, pos.z) * static_cast<float>(CHUNK_SIZE);

		faceShader.setVec3("chunkPosition", chunkWorldPos.x, chunkWorldPos.y, chunkWorldPos.z);

		chunk->render();
	}
}

void World::rebuildAllChunkMeshes()
{
	PROFILE_SCOPE("Build chunk meshes");

	meshBuildChunkContainer.clear();
	for (const auto& pair : chunks)
	{
		pair.second->buildMesh();
	}
}

void World::getChunkMeshesInfo(size_t& totalFaces, size_t& totalFaceCapacity, size_t& potentialMaximumCapacity)
{
	totalFaces = 0;
	totalFaceCapacity = 0;
	for (const auto& pair : chunks)
	{
		const Chunk* chunk = pair.second.get();
		totalFaces += chunk->getFaceCount();
		totalFaceCapacity += chunk->getFaceCapacity();
	}

	potentialMaximumCapacity = chunks.size() * CHUNK_VOLUME / 2 * 6;
}

void World::loadChunk(int chunkX, int chunkY, int chunkZ)
{
	// Check if chunk already exists
	Int3 chunkPos(chunkX, chunkY, chunkZ);
    if (chunks.find(chunkPos) != chunks.end())
    {
        return;
	}

	// Find existing neighbors
	Chunk* neighbors[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	{ // -X
		Int3 npos = chunkPos;
		npos.x--;
		auto it = chunks.find(npos);
		if (it != chunks.end())
		{
			neighbors[0] = it->second.get();
		}
	}
	{ // +X
		Int3 npos = chunkPos;
		npos.x++;
		auto it = chunks.find(npos);
		if (it != chunks.end())
		{
			neighbors[1] = it->second.get();
		}
	}
	{ // -Y
		Int3 npos = chunkPos;
		npos.y--;
		auto it = chunks.find(npos);
		if (it != chunks.end())
		{
			neighbors[2] = it->second.get();
		}
	}
	{ // +Y
		Int3 npos = chunkPos;
		npos.y++;
		auto it = chunks.find(npos);
		if (it != chunks.end())
		{
			neighbors[3] = it->second.get();
		}
	}
	{ // -Z
		Int3 npos = chunkPos;
		npos.z--;
		auto it = chunks.find(npos);
		if (it != chunks.end())
		{
			neighbors[4] = it->second.get();
		}
	}
	{ // +Z
		Int3 npos = chunkPos;
		npos.z++;
		auto it = chunks.find(npos);
		if (it != chunks.end())
		{
			neighbors[5] = it->second.get();
		}
	}

	// Create and initialize chunk
	auto chunk = chunkPool.acquire();
	chunk->init(chunkX, chunkY, chunkZ, neighbors);

	blocksBuildChunkContainer.insert(chunk.get());
	meshBuildChunkContainer.insert(chunk.get());

	chunks[chunk->getPosition()] = std::move(chunk);

	for (int i = 0; i < 6; i++)
	{
		Chunk* neighbor = neighbors[i];
		if (neighbor)
		{
			meshBuildChunkContainer.insert(neighbor);
		}
	}
}

void World::buildChunkBlocks()
{
	// Time for first load:
	// Single-threaded: 8.4 ms
	// Multi-threaded (12 threads) (min chunk size 1): 4.5 ms
	// Multi-threaded (10 threads) (min chunk size 500): 4.3 ms
	// Multi-threaded (5 threads) (min chunk size 1000): 4.33 ms
	// Multi-threaded (4 threads) (min chunk size 1500): 4.33 ms
	// Multi-threaded (3 threads) (min chunk size 2000): 4.8 ms

	PROFILE_SCOPE("Build chunk blocks");

	if (false)
	{
		for (Chunk* chunk : blocksBuildChunkContainer)
		{
			chunk->buildBlocks();
		}
		blocksBuildChunkContainer.clear();
	}
	else
	{
		// Convert set to vector for parallel processing
		std::vector<Chunk*> chunksToProcess(blocksBuildChunkContainer.begin(), blocksBuildChunkContainer.end());
		blocksBuildChunkContainer.clear();

		// Use parallel for to build blocks across multiple threads
		ParallelUtils::parallelForEach(chunksToProcess, 500, [](Chunk* chunk)
			{
				chunk->buildBlocks();
			});

		// TODO: Instead of waiting for a completion, chunks should be building between updates. When chunk will build its blocks, it will be allowed to build mesh.
	}
}

void World::buildChunkMeshes()
{
	PROFILE_SCOPE("Build chunk meshes");

	for (Chunk* chunk : meshBuildChunkContainer)
	{
		chunk->buildMesh();
	}
	meshBuildChunkContainer.clear();
}

std::unique_ptr<Chunk> World::ChunkPool::acquire()
{
	if (!pool.empty())
	{
		std::unique_ptr<Chunk> chunk = std::move(pool.back());
		pool.pop_back();
		return chunk;
	}
	return std::make_unique<Chunk>();
}

void World::ChunkPool::release(std::unique_ptr<Chunk> chunk)
{
	chunk->destroy();
	pool.push_back(std::move(chunk));
}
