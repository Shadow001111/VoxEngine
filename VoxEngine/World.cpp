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
		startBuildingChunkBlocks();
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

		if (chunk->getState() != Chunk::State::Ready)
		{
			continue;
		}

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
		Chunk* chunk = pair.second.get();
		if (chunk->getState() == Chunk::State::Ready)
		{
			chunk->buildMesh();
		}
	}
}

void World::debugMethod()
{
	int count[4] = { 0, 0, 0, 0 };
	for (const auto& pair : chunks)
	{
		auto state = pair.second->getState();
		size_t index = (size_t)state;
		count[index]++;
	}

	for (int i = 0; i < 4; i++)
	{
		std::cout << count[i] << " ";
	}
	std::cout << std::endl;
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
	Chunk* chunkPtr = chunk.get();

	// Add to blocks build queue
	{
		std::lock_guard<std::mutex> lock(blocksBuildMutex);
		blocksBuildChunkContainer.insert(chunkPtr);
	}

	chunks[chunk->getPosition()] = std::move(chunk);
}

void World::startBuildingChunkBlocks()
{
	// Maybe add PROFILE_SCOPE inside Chunk::buildBlocks. Make Profiler thread safe.
	PROFILE_SCOPE("Start building chunk blocks");

	// Collect chunks that need block building
	std::vector<Chunk*> chunksToProcess;
	{
		std::lock_guard<std::mutex> lock(blocksBuildMutex);
		if (blocksBuildChunkContainer.empty())
		{
			return;
		}

		chunksToProcess.reserve(blocksBuildChunkContainer.size());
		for (Chunk* chunk : blocksBuildChunkContainer)
		{
			chunk->setState(Chunk::State::BuildingBlocks);
			chunksToProcess.push_back(chunk);
		}
		blocksBuildChunkContainer.clear();
	}

	// Submit work to thread pool
	// TODO: Maybe batch for less mutex locking
	ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
	for (Chunk* chunk : chunksToProcess)
	{
		pool.enqueue([this, chunk]()
			{
				// Build blocks in background thread
				chunk->buildBlocks();

				// Mark as needing mesh
				chunk->setState(Chunk::State::NeedsMesh);

				std::lock_guard<std::mutex> lock(meshBuildMutex);
				meshBuildChunkContainer.insert(chunk);

				for (int i = 0; i < 6; i++)
				{
					Chunk* neighbor = chunk->neighbors[i];
					if (neighbor && neighbor->getState() == Chunk::State::Ready)
					{
						meshBuildChunkContainer.insert(neighbor);
					}
				}
			});
	}
}

void World::buildChunkMeshes()
{
	PROFILE_SCOPE("Build chunk meshes");

	// Collect chunks that need mesh building
	std::vector<Chunk*> chunksToProcess;
	{
		std::lock_guard<std::mutex> lock(meshBuildMutex);
		if (meshBuildChunkContainer.empty())
		{
			return;
		}

		std::unordered_set<Chunk*> remainingChunks;
		remainingChunks.reserve(meshBuildChunkContainer.size());

		chunksToProcess.reserve(meshBuildChunkContainer.size());
		for (Chunk* chunk : meshBuildChunkContainer)
		{
			Chunk::State state = chunk->getState();
			if (state == Chunk::State::NeedsMesh || state == Chunk::State::Ready)
			{
				chunksToProcess.push_back(chunk);
			}
			else
			{
				remainingChunks.insert(chunk);
			}
		}
		meshBuildChunkContainer.swap(remainingChunks);
	}

	// Build meshes on main thread (OpenGL calls)
	for (Chunk* chunk : chunksToProcess)
	{
		chunk->buildMesh();
		chunk->setState(Chunk::State::Ready);
	}
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
