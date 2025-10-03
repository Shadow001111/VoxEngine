#include "World.h"

#include "Profiler.h"

#include <iostream>

World::World()
{
}

World::~World()
{
}

void World::loadChunks(const Int3& chunkLoaderPos, int renderDistance)
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

void World::render(const Shader& faceShader) const
{
	for (const auto& pair : chunks)
	{
		const Chunk* chunk = pair.second.get();

		Int3 pos = chunk->getPosition();
		glm::vec3 chunkWorldPos = glm::vec3(pos.x, pos.y, pos.z) * static_cast<float>(CHUNK_SIZE);

		faceShader.setVec3("chunkPosition", chunkWorldPos.x, chunkWorldPos.y, chunkWorldPos.z);

		chunk->render();
	}
	glBindVertexArray(0); // Unbinding chunk's VAO for safety
}

void World::loadChunk(int chunkX, int chunkY, int chunkZ)
{
	// Check if chunk already exists
	Int3 chunkPos(chunkX, chunkY, chunkZ);
    if (chunks.find(chunkPos) != chunks.end())
    {
        return;
	}

	// Create and initialize chunk
	auto chunk = chunkPool.acquire();
	chunk->init(chunkX, chunkY, chunkZ);
	chunk->buildBlocks();
	chunk->buildMesh();
	chunks[chunk->getPosition()] = std::move(chunk);
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
