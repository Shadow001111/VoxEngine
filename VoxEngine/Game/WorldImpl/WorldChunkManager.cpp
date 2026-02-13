#include "WorldChunkManager.h"

#include "Core/Profiler.h"
#include "Core/Multithreading/ThreadPool.h"

#include "../World/ChunkLoaders/SphericalChunkLoader.h"

WorldChunkManager::WorldChunkManager()
{
	// Chunk loaders
	createChunkLoader<SphericalChunkLoader>();
}

void WorldChunkManager::preparation(size_t chunkCount)
{
	chunkPool.allocate(chunkCount + 10);

	size_t regionCount = (chunkCount * 2) / CHUNK_REGION_VOLUME + 1;
	chunkRegions.reserve(regionCount);
	chunkRegionPool.allocate(regionCount);
}

void WorldChunkManager::loadChunks(const glm::dvec3& playerPos, int chunkLoadingDistance)
{
	// Check if player chunk position changed
	glm::ivec3 chunkLoaderPos = glm::ivec3(glm::floor(playerPos)) >> CHUNK_SIZE_LOG2;
	if (lastChunkLoaderPos == chunkLoaderPos && lastChunkLoadingDistance == chunkLoadingDistance)
	{
		return;
	}
	lastChunkLoaderPos = chunkLoaderPos;
	lastChunkLoadingDistance = chunkLoadingDistance;

	// Update chunk loaders
	{
		PROFILE_SCOPE("Update chunk loaders", ProfileCategory::ChunkLoadUnload);
		for (auto& chunkLoader : chunkLoaders)
		{
			chunkLoader->update(chunkLoaderPos, chunkLoadingDistance);
		}
	}

	// Load chunks
	{
		PROFILE_SCOPE("Load chunks", ProfileCategory::ChunkLoadUnload);
		for (auto& chunkLoader : chunkLoaders)
		{
			const auto& positions = chunkLoader->getChunksToLoad();
			for (const auto& pos : positions)
			{
				loadChunk(pos);
			}
		}
	}

	// Unload chunks
	{
		PROFILE_SCOPE("Unload chunks", ProfileCategory::ChunkLoadUnload);
		for (auto& chunkLoader : chunkLoaders)
		{
			const auto& positions = chunkLoader->getChunksToUnload();
			for (const auto& pos : positions)
			{
				unloadChunk(pos);
			}
		}
	}
}

void WorldChunkManager::update()
{
	// Return processing chunks to the pool
	chunkPool.returnProcessingChunksToPool();

	// Start building chunk blocks
	if (!buildContainers.blocks.empty())
	{
		startBuildingChunkBlocks();
	}

	// Update chunks structure blocks
	if (Chunk::gHasStructureBlockChanges.load(std::memory_order_acquire))
	{
		Chunk::gHasStructureBlockChanges.store(false, std::memory_order_release);

		PROFILE_SCOPE("Update chunk blocks", ProfileCategory::ChunkBlocks);

		for (const auto& [_, chunkRegion] : chunkRegions)
		{
			for (Chunk* chunk : chunkRegion->chunks)
			{
				if (!chunk) continue;

				if (chunk->areBlocksBuilt() && chunk->hasStructureBlockUpdates())
				{
					chunk->updateStructureBlocks();
				}
			}
		}
	}

	// Start building chunk lights
	if (!buildContainers.lights.empty())
	{
		startBuildingChunkLights();
	}

	// Update chunks lights
	// Sometimes loops forever. Because of sky light propagation.
	// If not limited, it goes forever. But when limited to 20 iterations, next fame iteration count is low and less than 20. STRANGE.
	{
		size_t iterations = 0;
		collectChunksNeedingLightUpdate();
		while (!buildContainers.lightUpdateA.empty() || !buildContainers.lightUpdateB.empty())
		{
			updateChunkLights();
			iterations++;
			if (iterations >= 10)
			{
				break;
			}
			collectChunksNeedingLightUpdate();
		}
	}

	// Update chunks meshes
	updateChunkMeshes();
}

void WorldChunkManager::sendChunkMeshesToGPU()
{
	// Send only dirty meshes
	if (ChunkMesh::getHasPendingMeshUploads())
	{
		ChunkMesh::setHasPendingMeshUploads(false);

		PROFILE_SCOPE("Check dirty meshes to send to GPU", ProfileCategory::ChunkMesh);

		for (const auto& [_, chunkRegion] : chunkRegions)
		{
			for (Chunk* chunk : chunkRegion->chunks)
			{
				if (!chunk) continue;

				chunk->askForMeshUpload();
			}
		}
	}
	ChunkMesh::sendMeshesToGPU();
}

Chunk* WorldChunkManager::getChunkAt(const glm::ivec3& position) const
{
	// Get region position
	glm::ivec3 regionPos = ChunkRegion::getRegionPosition(position);

	// Check if region exists
	auto it = chunkRegions.find(regionPos);
	if (it == chunkRegions.end())
	{
		return nullptr;
	}

	// Region exists
	ChunkRegion* chunkRegion = it->second;

	// Get chunk index in region
	size_t index = ChunkRegion::getChunkIndexInRegion(position);

	// Return chunk pointer
	return chunkRegion->chunks[index];
}

bool WorldChunkManager::chunkExistsAt(const glm::ivec3& position) const
{
	// Get region position
	glm::ivec3 regionPos = ChunkRegion::getRegionPosition(position);

	// Check if region exists
	auto it = chunkRegions.find(regionPos);
	if (it == chunkRegions.end())
	{
		return false;
	}

	// Region exists
	ChunkRegion* chunkRegion = it->second;

	// Get chunk index in region
	size_t index = ChunkRegion::getChunkIndexInRegion(position);

	// Return result
	return chunkRegion->chunks[index] != nullptr;
}

void WorldChunkManager::startBuildingChunkBlocks()
{
	// Collect chunks
	std::vector<Chunk*> chunksToProcess;
	{
		PROFILE_SCOPE("Collect chunks for block building", ProfileCategory::ChunkBlocks);

		std::lock_guard<std::mutex> lock(buildContainers.blocksMutex);
		if (buildContainers.blocks.empty())
		{
			return;
		}

		chunksToProcess.reserve(buildContainers.blocks.size());
		for (Chunk* chunk : buildContainers.blocks)
		{
			if (chunk->getState() != Chunk::State::NotInitialized_NeedsBlocks)
			{
				continue;
			}
			ASSERT(!chunk->areBlocksBuilt());
			chunk->setState(Chunk::State::BuildingBlocks);
			chunksToProcess.push_back(chunk);
		}
		buildContainers.blocks.clear();
	}

	// Submit chunks to thread pool
	{
		PROFILE_SCOPE("Send chunks to block building", ProfileCategory::ChunkBlocks);

		ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
		for (Chunk* chunk : chunksToProcess)
		{
			pool.enqueue([this, chunk]()
				{
					chunk->buildBlocks();
					if (!chunk->getIsLoadedInWorld())
					{
						return;
					}

					chunk->setState(Chunk::State::NeedsLight);

					{
						std::lock_guard<std::mutex> lock(buildContainers.lightsMutex);
						buildContainers.lights.insert(chunk);
					}
				});
		}
	}
}

void WorldChunkManager::startBuildingChunkLights()
{
	// Collect chunks that are ready for light building
	std::vector<Chunk*> chunksToProcess;
	{
		PROFILE_SCOPE("Collect chunks for light building", ProfileCategory::ChunkLight);

		std::lock_guard<std::mutex> lock(buildContainers.lightsMutex);
		if (buildContainers.lights.empty())
		{
			return;
		}

		const size_t chunkCount = buildContainers.lights.size();

		robin_hood::unordered_flat_set<Chunk*> remainingChunks;
		remainingChunks.reserve(chunkCount);

		chunksToProcess.reserve(chunkCount);
		for (Chunk* chunk : buildContainers.lights)
		{
			// Should always pass the test, but sometimes it doesn't
			if (chunk->getState() != Chunk::State::NeedsLight)
			{
				continue;
			}

			ASSERT(chunk->areBlocksBuilt());
			ASSERT(!chunk->isLightBuilt());

			// Check if all neighbors have blocks built
			bool allNeighborsReady = true;
			for (int i = 0; i < 6; i++)
			{
				const Chunk* neighbor = chunk->neighbors[i];
				if (neighbor && !neighbor->areBlocksBuilt())
				{
					allNeighborsReady = false;
					break;
				}
			}

			if (allNeighborsReady)
			{
				chunk->setState(Chunk::State::BuildingLight);
				chunksToProcess.push_back(chunk);
			}
			else
			{
				// Keep in container, waiting for neighbors
				remainingChunks.insert(chunk);
			}
		}

		buildContainers.lights.swap(remainingChunks);
	}

	// Submit chunks to thread pool
	{
		PROFILE_SCOPE("Send chunks to light building", ProfileCategory::ChunkLight);

		ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
		for (Chunk* chunk : chunksToProcess)
		{
			pool.enqueue([this, chunk]()
				{
					chunk->buildLight();
				});
		}
	}
}

void WorldChunkManager::collectChunksNeedingLightUpdate()
{
	PROFILE_SCOPE("Collect chunks needing light update", ProfileCategory::ChunkLight);
	// TODO: Have single container, but add chunks from different ends, depending on group

	buildContainers.lightUpdateA.clear();
	buildContainers.lightUpdateB.clear();

	for (const auto& [_, chunkRegion] : chunkRegions)
	{
		for (Chunk* chunk : chunkRegion->chunks)
		{
			if (!chunk) continue;

			if (chunk->hasLightUpdates())
			{
				const auto& chunkPosition = chunk->getPosition();
				if ((chunkPosition.x ^ chunkPosition.y ^ chunkPosition.z) & 1)
				{
					buildContainers.lightUpdateA.push_back(chunk);
				}
				else
				{
					buildContainers.lightUpdateB.push_back(chunk);
				}
			}
		}
	}
}

void WorldChunkManager::updateChunkLights()
{
	// Using parallelForEach because it will assure that all tasks are done before returning
	// Update chunks in two separate passes in checkboard pattern to avoid racing conditions
	ParallelUtils::parallelForEach(buildContainers.lightUpdateA, 1, [](Chunk* chunk)
		{
			chunk->updateLight();
		});
	ParallelUtils::parallelForEach(buildContainers.lightUpdateB, 1, [](Chunk* chunk)
		{
			chunk->updateLight();
		});
}

void WorldChunkManager::updateChunkMeshes()
{
	ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
	for (const auto& [_, chunkRegion] : chunkRegions)
	{
		for (Chunk* chunk : chunkRegion->chunks)
		{
			if (!chunk) continue;

			if (chunk->shouldMeshBeUpdated())
			{
				pool.enqueue([chunk]()
					{
						chunk->updateMesh();
					});
			}
		}
	}
}

void WorldChunkManager::rebuildAllChunkMeshes()
{
	for (const auto& [_, chunkRegion] : chunkRegions)
	{
		for (Chunk* chunk : chunkRegion->chunks)
		{
			if (!chunk) continue;

			chunk->markMeshDirty();
		}
	}
}

void WorldChunkManager::loadChunk(const glm::ivec3& position)
{
	// Get region position
	glm::ivec3 regionPos = ChunkRegion::getRegionPosition(position);

	// Check if region already exists
	ChunkRegion* region;
	auto it = chunkRegions.find(regionPos);
	if (it != chunkRegions.end())
	{
		// Region already exists, use it
		region = it->second;
	}
	else
	{
		// Region doesn't exist, create it
		// Aquire region from pool
		region = chunkRegionPool.acquire();

		// Add region to map
		chunkRegions.emplace(regionPos, region);
	}

	// Get chunk index in region
	size_t index = ChunkRegion::getChunkIndexInRegion(position);

	// Check if chunk already exists
	if (region->chunks[index])
	{
		// If loaded, it should just add loader to existent chunk, but we don't need it right now.
		return;
	}

	// Find existing neighbors
	Chunk* neighbors[6] = {
		getChunkAt({ position.x - 1, position.y,	 position.z		}),
		getChunkAt({ position.x + 1, position.y,	 position.z		}),
		getChunkAt({ position.x,	 position.y - 1, position.z		}),
		getChunkAt({ position.x,	 position.y + 1, position.z		}),
		getChunkAt({ position.x,	 position.y,	 position.z - 1 }),
		getChunkAt({ position.x,	 position.y,	 position.z + 1 })
	};

	// Create and initialize chunk
	Chunk* chunk = chunkPool.acquire();
	chunk->addLoader();
	chunk->init(position, neighbors);

	// Send chunk to building blocks
	{
		std::lock_guard<std::mutex> lock(buildContainers.blocksMutex);
		buildContainers.blocks.insert(chunk);
	}

	// Add chunk to the region
	region->chunks[index] = chunk;

	// Increase chunk count in region
	region->chunkCount++;
	ASSERT(region->chunkCount <= CHUNK_REGION_VOLUME);
}

void WorldChunkManager::unloadChunk(const glm::ivec3& position)
{
	// Get region position
	glm::ivec3 regionPos = ChunkRegion::getRegionPosition(position);

	// Check if region exists
	auto it = chunkRegions.find(regionPos);
	if (it == chunkRegions.end())
	{
		return;
	}

	// Region exists
	ChunkRegion* chunkRegion = it->second;

	// Get chunk index in region
	size_t index = ChunkRegion::getChunkIndexInRegion(position);

	// Check if chunk exists
	Chunk* chunk = chunkRegion->chunks[index];
	if (!chunk)
	{
		return;
	}

	// Decrement loader count
	chunk->removeLoader();

	// Release chunk to pool (will call Chunk::destroy)
	chunkPool.release(chunk);

	// Remove chunk from region
	chunkRegion->chunks[index] = nullptr;

	// Decrease chunk count in region
	ASSERT(chunkRegion->chunkCount > 0);
	chunkRegion->chunkCount--;

	// If region is empty, remove it from map and return to the pool
	if (chunkRegion->chunkCount == 0)
	{
		chunkRegions.erase(it);
		chunkRegionPool.release(chunkRegion);
	}
}
