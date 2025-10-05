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
	unloadChunksOutsideRange(renderDistance);

	// Load chunks in a cubic area around the chunkLoaderPos
	// TODO: Make area spherical
	{
		PROFILE_SCOPE("Load chunks");

		static std::vector<Chunk*> chunksToSend;
		chunksToSend.clear();

		for (int x = -renderDistance; x <= renderDistance; x++)
		{
			int chunkX = chunkLoaderPos.x + x;
			for (int y = -renderDistance; y <= renderDistance; y++)
			{
				int chunkY = chunkLoaderPos.y + y;
				for (int z = -renderDistance; z <= renderDistance; z++)
				{
					int chunkZ = chunkLoaderPos.z + z;
					loadChunk(chunkX, chunkY, chunkZ, chunksToSend);
				}
			}
		}

		{
			std::lock_guard<std::mutex> lock(blocksBuildMutex);
			for (Chunk* chunkPtr : chunksToSend)
			{
				blocksBuildChunkContainer.insert(chunkPtr);
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
		startBuildingChunkMeshes();
	}

	// Process all pending mesh uploads on main thread
	Chunk::sendMeshesToGPU();
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
	PROFILE_SCOPE("Rebuild all chunk meshes");

	// Queue all ready chunks for mesh rebuild
	{
		std::lock_guard<std::mutex> lock(meshBuildMutex);
		meshBuildChunkContainer.clear();
		for (const auto& pair : chunks)
		{
			Chunk* chunk = pair.second.get();
			if (chunk->getState() == Chunk::State::Ready)
			{
				chunk->setState(Chunk::State::NeedsMesh);
				meshBuildChunkContainer.insert(chunk);
			}
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

Chunk* World::getChunkAt(const Int3& position) const
{
	auto it = chunks.find(position);
	if (it == chunks.end())
	{
		return nullptr;
	}
	return it->second.get();
}

Chunk* World::getChunkAt(int x, int y, int z) const
{
	auto it = chunks.find(Int3(x, y, z));
	if (it == chunks.end())
	{
		return nullptr;
	}
	return it->second.get();
}

bool World::chunkExistsAt(const Int3& position) const
{
	return chunks.find(position) != chunks.end();
}

bool World::chunkExistsAt(int x, int y, int z) const
{
	return chunks.find(Int3(x, y, z)) != chunks.end();
}

void World::unloadChunksOutsideRange(int renderDistance)
{
	std::vector<Int3> chunksToUnload;
	{
		const int x = lastChunkLoaderPos.x;
		const int y = lastChunkLoaderPos.y;
		const int z = lastChunkLoaderPos.z;

		PROFILE_SCOPE("Unload chunks: collect");
		for (const auto& pair : chunks)
		{
			const Int3& pos = pair.first;
			if (std::abs(pos.x - x) > renderDistance ||
				std::abs(pos.y - y) > renderDistance ||
				std::abs(pos.z - z) > renderDistance)
			{
				chunksToUnload.push_back(pos);
			}
		}
	}
	{
		PROFILE_SCOPE("Unload chunks: unload");
		for (const Int3& pos : chunksToUnload)
		{
			auto it = chunks.find(pos);

			const Chunk* chunk = it->second.get();
			Chunk::State state = chunk->getState();
			bool isBeingProcessed = chunk->isBeingProcessed();

			// TODO: Should stop processing chunk
			// Maybe chunk pool shouldn't return processing chunk to the pool.
			// It should store it to some vector, where it will be checked, it will be returned to the pool when done processing.
			if (state != Chunk::State::Ready)
			{
				std::cout << (size_t)state << " " << isBeingProcessed << std::endl;
			}

			chunkPool.release(std::move(it->second));
			chunks.erase(it);
		}
	}
}

void World::loadChunk(int chunkX, int chunkY, int chunkZ, std::vector<Chunk*>& chunksToSend)
{
	// Check if chunk already exists
	Int3 chunkPosition = { chunkX, chunkY, chunkZ };
    if (chunkExistsAt(chunkPosition))
    {
        return;
	}

	// Find existing neighbors
	Chunk* neighbors[6] = {
		getChunkAt(chunkX - 1, chunkY, chunkZ),
		getChunkAt(chunkX + 1, chunkY, chunkZ),
		getChunkAt(chunkX, chunkY - 1, chunkZ),
		getChunkAt(chunkX, chunkY + 1, chunkZ),
		getChunkAt(chunkX, chunkY, chunkZ - 1),
		getChunkAt(chunkX, chunkY, chunkZ + 1)
	};

	// Create and initialize chunk
	auto chunk = chunkPool.acquire();
	chunk->init(chunkX, chunkY, chunkZ, neighbors);

	chunksToSend.push_back(chunk.get());

	chunks.emplace(chunkPosition, std::move(chunk)); // Takes much time
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

void World::startBuildingChunkMeshes()
{
	// Collect chunks that need mesh building
	std::vector<Chunk*> chunksToProcess;
	{
		PROFILE_SCOPE("Collect chunks for mesh building");

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
				chunk->setState(Chunk::State::BuildingMesh);
				chunksToProcess.push_back(chunk);
			}
			else
			{
				remainingChunks.insert(chunk);
			}
		}
		meshBuildChunkContainer.swap(remainingChunks);
	}

	// Submit mesh building to thread pool
	{
		PROFILE_SCOPE("Sumbit chunks to mesh building");

		ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
		for (Chunk* chunk : chunksToProcess)
		{
			pool.enqueue([chunk]()
				{
					// Build mesh in background thread (no OpenGL calls here)
					chunk->buildMesh();

					// Mark as chunk as Ready. His mesh can be not on the GPU yet.
					chunk->setState(Chunk::State::Ready);

					// TODO: Issue: Chunk's mesh if flickering
				});
		}
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
