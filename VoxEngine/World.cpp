#include "World.h"

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
	lastChunkLoaderPos = chunkLoaderPos;

	// Unload chunks that are out of range
	{
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
			chunks.erase(pos);
		}
	}

	// Load chunks in a cubic area around the chunkLoaderPos
	// TODO: Make area spherical
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

    std::cout << chunks.size() << std::endl;
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
	auto chunk = std::make_unique<Chunk>();
	chunk->init(chunkX, chunkY, chunkZ);
	chunk->buildBlocks();
	chunk->buildMesh();
	chunks[chunk->getPosition()] = std::move(chunk);
}
