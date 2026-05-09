#include "WorldChunkManager.h"

#include "Game/TracyProfiler.h"
#include "Core/Multithreading/ThreadPool.h"

#include "Game/ProfileCategories.h"

#include "../World/ChunkLoaders/SphericalChunkLoader.h"

#include "../World/ChunkRegionManager.h"

#include <iostream>

constexpr size_t CHUNKS_PER_BATCH = 16;

WorldChunkManager::~WorldChunkManager()
{
	// Unload all chunks
	for (const auto& [_, chunkRegion] : Chunk::managerInstances->chunkRegion.getRegionMap())
	{
		for (Chunk* chunk : chunkRegion->chunks)
		{
			if (!chunk) continue;
			chunk->destroy();
		}
	}

	Chunk::globalDestroy();
}

void WorldChunkManager::init()
{
	// Chunk loaders
	createChunkLoader<SphericalChunkLoader>();

	//
	Chunk::globalInit();
}

void WorldChunkManager::preparation(size_t chunkCount)
{
	chunkPool.allocate(chunkCount + 10);

	size_t regionCount = (chunkCount * 2) / CHUNK_REGION_VOLUME + 1;
	Chunk::managerInstances->chunkRegion.preparation(regionCount);
}

void WorldChunkManager::loadChunksAroundPlayer(const glm::dvec3& playerPos, int chunkLoadingDistance)
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
		TRACY_SCOPE_NC("Update chunk loaders", ProfileCategory::ChunkLoadUnload);
		for (auto& chunkLoader : chunkLoaders)
		{
			chunkLoader->update(chunkLoaderPos, chunkLoadingDistance);
		}
	}

	// Load chunks
	{
		TRACY_SCOPE_NC("Load chunks around player", ProfileCategory::ChunkLoadUnload);
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
		TRACY_SCOPE_NC("Unload chunks far from player", ProfileCategory::ChunkLoadUnload);
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
	startBuildingChunkBlocks();

	// Update chunks structure blocks
	if (Chunk::managerInstances->structureBlock.hasAnyChanges.exchange(false, std::memory_order_acquire))
	{
		for (const auto& [_, chunkRegion] : Chunk::managerInstances->chunkRegion.getRegionMap())
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
	startBuildingChunkLights();

	// Update chunks lights
	for (int i = 0; i < 40; i++)
	{
		collectChunksForLightUpdate();

		if (buildContainers.lightUpdateA.empty() && buildContainers.lightUpdateB.empty())
		{
			break;
		}

		updateChunkLights();
	}

	// Update chunks meshes
	updateChunkMeshes();

	// Update chunks connectivity
	if constexpr (Chunk::USE_CONNECTIVITY_TESTING)
	{
		updateChunkConnectivity();
	}
}

void WorldChunkManager::sendChunkMeshesToGPU()
{
	// Send only dirty meshes
	if (!ChunkRegion::readAndSetGlobalFlag(ChunkRegion::Flag::HasMeshToUpload, false))
	{
		return;
	}

	{
		TRACY_SCOPE_NC("Check dirty meshes to send to GPU", ProfileCategory::ChunkMesh);

		for (const auto& [_, chunkRegion] : Chunk::managerInstances->chunkRegion.getRegionMap())
		{
			if (!chunkRegion->readAndSetFlag(ChunkRegion::Flag::HasMeshToUpload, false))
			{
				continue;
			}

			for (Chunk* chunk : chunkRegion->chunks)
			{
				if (!chunk) continue;

				chunk->askForMeshUpload();
			}
		}
	}

	ChunkMesh::sendMeshesToGPU();
}

Chunk* WorldChunkManager::getChunkAt(const glm::ivec3& chunkPosition) const
{
	// Get region position
	glm::ivec3 regionPosition = ChunkRegion::getRegionPosition(chunkPosition);

	// Get chunk region if exists
	ChunkRegion* chunkRegion = Chunk::managerInstances->chunkRegion.getRegion(regionPosition);
	if (!chunkRegion)
	{
		return nullptr;
	}

	// Get chunk index in region
	size_t index = ChunkRegion::getChunkIndexInRegion(chunkPosition);

	// Return chunk pointer
	return chunkRegion->chunks[index];
}

bool WorldChunkManager::chunkExistsAt(const glm::ivec3& chunkPosition) const
{
	// Get region position
	glm::ivec3 regionPosition = ChunkRegion::getRegionPosition(chunkPosition);

	// Get chunk region if exists
	const ChunkRegion* chunkRegion = Chunk::managerInstances->chunkRegion.getRegion(regionPosition);
	if (!chunkRegion)
	{
		return false;
	}

	// Get chunk index in region
	size_t index = ChunkRegion::getChunkIndexInRegion(chunkPosition);

	// Return result
	return chunkRegion->chunks[index] != nullptr;
}

void WorldChunkManager::startBuildingChunkBlocks()
{
	TRACY_SCOPE_NC("Start building chunk blocks", ProfileCategory::ChunkLight);

	// Validate and collect chunks
	chunksToProcess.clear();
	{
		TRACY_SCOPE_NC("Validate chunks for block building", ProfileCategory::ChunkBlocks);

		size_t chunkCount = buildContainers.blocks.size();
		for (size_t i = 0; i < chunkCount;)
		{
			Chunk* chunk = buildContainers.blocks[i];
			if (chunk->getState() == Chunk::State::NotInitialized_NeedsBlocks)
			{
				i++;
			}
			else
			{
				std::swap(chunk, buildContainers.blocks.back());
				chunkCount--;
			}
		}

		buildContainers.blocks.resize(chunkCount);

		chunksToProcess.swap(buildContainers.blocks);

		buildContainers.blocks.clear();
	}

	// Submit chunks to thread pool
	if (!chunksToProcess.empty())
	{
		TRACY_SCOPE_NC("Send chunks to block building", ProfileCategory::ChunkBlocks);

		ThreadPool& pool = ParallelUtils::getGlobalThreadPool();

		const size_t chunkCount = chunksToProcess.size();

		std::vector<WorkerThread::Task> tasks;
		tasks.reserve((chunkCount + CHUNKS_PER_BATCH - 1) / CHUNKS_PER_BATCH);

		for (size_t i = 0; i < chunkCount; i += CHUNKS_PER_BATCH)
		{
			size_t batchEnd = std::min(i + CHUNKS_PER_BATCH, chunkCount);
			std::vector<Chunk*> batch(chunksToProcess.begin() + i, chunksToProcess.begin() + batchEnd);

			tasks.emplace_back([this, batch_ = std::move(batch)]()
				{
					for (Chunk* chunk : batch_)
					{
						chunk->setState(Chunk::State::BuildingBlocks);
						chunk->buildBlocks();
					}

					{
						std::lock_guard lock(buildContainers.lightsMutex);
						buildContainers.lightsIncoming.insert(batch_.begin(), batch_.end());
					}
				});
		}

		pool.enqueueBulk(std::move(tasks));
	}
	chunksToProcess.clear();
}

void WorldChunkManager::startBuildingChunkLights()
{
	TRACY_SCOPE_NC("Start building chunk lights", ProfileCategory::ChunkLight);

	chunksToProcess.clear();

	// Swap/move incoming chunks to a local temporary set
	static robin_hood::unordered_flat_set<Chunk*> localIncoming;
	localIncoming.clear();
	{
		TRACY_SCOPE_NC("Sync Light Containers", ProfileCategory::ChunkLight);
		std::lock_guard lock(buildContainers.lightsMutex);
		if (!buildContainers.lightsIncoming.empty())
		{
			localIncoming.swap(buildContainers.lightsIncoming);
		}
	}

	// Merge new incoming chunks into our long-term processing set
	if (!localIncoming.empty())
	{
		TRACY_SCOPE_NC("Merge chunk sets", ProfileCategory::ChunkLight);
		buildContainers.lightsProcessing.insert(localIncoming.begin(), localIncoming.end());
	}

	if (buildContainers.lightsProcessing.empty()) return;

	// Process the local set
	{
		TRACY_SCOPE_NC("Check Ready Chunks", ProfileCategory::ChunkLight);

		// We use an iterator to erase chunks as they become ready
		for (auto it = buildContainers.lightsProcessing.begin(); it != buildContainers.lightsProcessing.end();)
		{
			Chunk* chunk = *it;

			if (!chunk->areBlocksBuilt() || chunk->isLightBuilt())
			{
				it = buildContainers.lightsProcessing.erase(it);
				continue;
			}

			// Check neighbors
			bool allNeighborsReady = true;
			const auto& neighbors = chunk->getNeighbors();
			for (int i = 0; i < 6; i++)
			{
				const Chunk* neighbor = neighbors[Chunk::getSideNeighborIndex(i)];
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
				it = buildContainers.lightsProcessing.erase(it); // Remove from processing
			}
			else
			{
				++it; // Keep in processing for next frame
			}
		}
	}

	// Submit chunks to thread pool
	if (!chunksToProcess.empty())
	{
		TRACY_SCOPE_NC("Send chunks to light building", ProfileCategory::ChunkLight);
		ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
		const size_t chunkCount = chunksToProcess.size();

		std::vector<WorkerThread::Task> tasks;
		tasks.reserve((chunkCount + CHUNKS_PER_BATCH - 1) / CHUNKS_PER_BATCH);

		for (size_t i = 0; i < chunkCount; i += CHUNKS_PER_BATCH)
		{
			size_t batchEnd = std::min(i + CHUNKS_PER_BATCH, chunkCount);
			std::vector<Chunk*> batch(chunksToProcess.begin() + i, chunksToProcess.begin() + batchEnd);

			tasks.emplace_back([batch_ = std::move(batch)]()
				{
					for (Chunk* chunk : batch_)
						chunk->buildLight();
				});
		}

		pool.enqueueBulk(std::move(tasks));
	}
	chunksToProcess.clear();
}

void WorldChunkManager::collectChunksForLightUpdate()
{
	buildContainers.lightUpdateA.clear();
	buildContainers.lightUpdateB.clear();

	if (!ChunkRegion::readAndSetGlobalFlag(ChunkRegion::Flag::HasLightToUpdate, false))
	{
		return;
	}

	TRACY_SCOPE_NC("Collect chunks for light update", ProfileCategory::ChunkLight);

	for (const auto& [_, chunkRegion] : Chunk::managerInstances->chunkRegion.getRegionMap())
	{
		if (!chunkRegion->readAndSetFlag(ChunkRegion::Flag::HasLightToUpdate, false))
		{
			continue;
		}

		for (Chunk* chunk : chunkRegion->chunks)
		{
			if (!(chunk && chunk->shouldUpdateLight())) continue;

			glm::ivec3 chunkPosition = chunk->getPosition();

			bool sector = (chunkPosition.x ^ chunkPosition.y ^ chunkPosition.z) & 1;

			auto& updateGroupVector = sector ? buildContainers.lightUpdateA : buildContainers.lightUpdateB;

			updateGroupVector.push_back(chunk);
		}
	}
}

void WorldChunkManager::updateChunkLights()
{
	TRACY_SCOPE_NC("Update chunk lights", ProfileCategory::ChunkLight);

	// Using parallelForEach because it will assure that all tasks are done before returning
	// Update chunks in two separate passes in checkboard pattern to make less iterations
	// Could make this non-blocking, but I don't want to get more work because of it (idk what's better)
	if (!buildContainers.lightUpdateA.empty())
	{
		ParallelUtils::parallelForEach(buildContainers.lightUpdateA, 1, [](Chunk* chunk)
			{
				chunk->updateLight();
			});
	}
	if (!buildContainers.lightUpdateB.empty())
	{
		ParallelUtils::parallelForEach(buildContainers.lightUpdateB, 1, [](Chunk* chunk)
			{
				chunk->updateLight();
			});
	}
}

void WorldChunkManager::updateChunkMeshes()
{
	// Check global flag
	if (!ChunkRegion::readAndSetGlobalFlag(ChunkRegion::Flag::HasMeshToUpdate, false))
	{
		return;
	}

	//
	chunksToProcess.clear();

	// Collect chunks
	{
		TRACY_SCOPE_NC("Collect chunks for mesh updating", ProfileCategory::ChunkMesh);

		for (const auto& [_, chunkRegion] : Chunk::managerInstances->chunkRegion.getRegionMap())
		{
			if (!chunkRegion->readAndSetFlag(ChunkRegion::Flag::HasMeshToUpdate, false))
			{
				continue;
			}

			for (Chunk* chunk : chunkRegion->chunks)
			{
				if (chunk && chunk->shouldUpdateMesh())
				{
					chunksToProcess.push_back(chunk);
				}
			}
		}
	}

	// Send to thread
	if (chunksToProcess.empty())
	{
		return;
	}

	ThreadPool& pool = ParallelUtils::getGlobalThreadPool();

	const size_t chunkCount = chunksToProcess.size();
	for (size_t i = 0; i < chunkCount; i += CHUNKS_PER_BATCH)
	{
		size_t batchEnd = std::min(i + CHUNKS_PER_BATCH, chunkCount);
		std::vector<Chunk*> batch(chunksToProcess.begin() + i, chunksToProcess.begin() + batchEnd);

		pool.enqueue([batch_ = std::move(batch)]()
			{
				for (Chunk* chunk : batch_)
				{
					chunk->updateMesh();
				}
			});
	}

	chunksToProcess.clear();
}

void WorldChunkManager::updateChunkConnectivity()
{
	// Check global flag
	if (!ChunkRegion::readAndSetGlobalFlag(ChunkRegion::Flag::HasConnectivityToUpdate, false))
	{
		return;
	}

	//
	chunksToProcess.clear();

	// Collect chunks
	{
		TRACY_SCOPE_NC("Collect chunks for connectivity updating", ProfileCategory::General);

		for (const auto& [_, chunkRegion] : Chunk::managerInstances->chunkRegion.getRegionMap())
		{
			if (!chunkRegion->readAndSetFlag(ChunkRegion::Flag::HasConnectivityToUpdate, false))
			{
				continue;
			}

			for (Chunk* chunk : chunkRegion->chunks)
			{
				if (chunk && chunk->shouldUpdateConnectivity())
				{
					chunksToProcess.push_back(chunk);
				}
			}
		}
	}

	// Send to thread
	if (chunksToProcess.empty())
	{
		return;
	}

	ThreadPool& pool = ParallelUtils::getGlobalThreadPool();

	const size_t chunkCount = chunksToProcess.size();
	for (size_t i = 0; i < chunkCount; i += CHUNKS_PER_BATCH)
	{
		size_t batchEnd = std::min(i + CHUNKS_PER_BATCH, chunkCount);
		std::vector<Chunk*> batch(chunksToProcess.begin() + i, chunksToProcess.begin() + batchEnd);

		pool.enqueue([batch_ = std::move(batch)]()
			{
				for (Chunk* chunk : batch_)
				{
					chunk->updateConnectivity();
				}
			});
	}

	chunksToProcess.clear();
}

void WorldChunkManager::rebuildAllChunkMeshes()
{
	for (const auto& [_, chunkRegion] : Chunk::managerInstances->chunkRegion.getRegionMap())
	{
		for (Chunk* chunk : chunkRegion->chunks)
		{
			if (!chunk) continue;

			chunk->markAsShouldUpdateMesh();
		}
	}
}

void WorldChunkManager::loadChunk(const glm::ivec3& chunkPosition)
{
	// Get region
	glm::ivec3 regionPosition = ChunkRegion::getRegionPosition(chunkPosition);
	ChunkRegion*  region = Chunk::managerInstances->chunkRegion.getOrCreateRegion(regionPosition);
	size_t index = ChunkRegion::getChunkIndexInRegion(chunkPosition);

	// Check if chunk already exists
	if (region->chunks[index])
	{
		// If loaded, it should just add loader to existent chunk, but we don't need it right now
		return;
	}

	// Find existing neighbors
	std::array<Chunk*, 27> neighbors{ nullptr };
	collectChunkNeighbors(chunkPosition, neighbors);

	// Create and initialize chunk
	Chunk* chunk = chunkPool.acquire();
	neighbors[Chunk::getNeighborIndex(0, 0, 0)] = chunk;
	chunk->addLoader();
	chunk->init(chunkPosition, neighbors, region);

	// Send chunk to building blocks
	buildContainers.blocks.push_back(chunk);

	// Add chunk to the region
	region->chunks[index] = chunk;

	// Increase chunk count in region
	region->chunkCount++;
}

void WorldChunkManager::unloadChunk(const glm::ivec3& chunkPosition)
{
	// Get region position
	glm::ivec3 regionPosition = ChunkRegion::getRegionPosition(chunkPosition);

	// Get chunk region if exists
	ChunkRegion* chunkRegion = Chunk::managerInstances->chunkRegion.getRegion(regionPosition);
	if (!chunkRegion) [[unlikely]]
	{
		return;
	}

	// Get chunk index in region
	size_t index = ChunkRegion::getChunkIndexInRegion(chunkPosition);

	// Check if chunk exists
	Chunk* chunk = chunkRegion->chunks[index];
	if (!chunk) [[unlikely]]
	{
		return;
	}

	// Decrement loader count
	chunk->removeLoader();

	// Destroy chunk
	chunk->destroy();

	// Release chunk to pool
	chunkPool.release(chunk);

	// Remove chunk from region
	chunkRegion->chunks[index] = nullptr;

	// Decrease chunk count in region
	chunkRegion->chunkCount--;

	// If region is empty, remove it from map and return to the pool
	if (chunkRegion->chunkCount == 0)
	{
		Chunk::managerInstances->chunkRegion.destroyChunkRegion(regionPosition);
	}
}

void WorldChunkManager::collectChunkNeighbors(const glm::ivec3& chunkPosition, std::array<Chunk*, 27>& neighbors) const
{
	TRACY_SCOPE_NC("Collect chunk neighbors", ProfileCategory::ChunkLoadUnload);

	constexpr int selfIndex = Chunk::getNeighborIndex(0, 0, 0);

	// Fast Path: Check if the chunk is internal (not on a region boundary)
	const glm::ivec3 centerRegPos = ChunkRegion::getRegionPosition(chunkPosition);
	const bool isInternal =
		ChunkRegion::getRegionPosition(chunkPosition - 1) == centerRegPos &&
		ChunkRegion::getRegionPosition(chunkPosition + 1) == centerRegPos;

	if (isInternal)
	{
		ChunkRegion* r = Chunk::managerInstances->chunkRegion.getRegion(centerRegPos);
		if (!r) return;

		for (int i = 0; i < 27; i++)
		{
			if (i == selfIndex) [[unlikely]] continue;
			const glm::ivec3 neighborPos = chunkPosition + Chunk::getNeighborOffset(i);
			neighbors[i] = r->chunks[ChunkRegion::getChunkIndexInRegion(neighborPos)];
		}
		return;
	}

	// Slow Path: Use bit-combining to index cache
	ChunkRegion* regionCache[8] = { nullptr };
	uint8_t cacheMask = 0; // Tracks which of the 8 slots are filled

	for (int i = 0; i < 27; i++)
	{
		if (i == selfIndex) [[unlikely]] continue;

		const glm::ivec3 neighborPos = chunkPosition + Chunk::getNeighborOffset(i);
		const glm::ivec3 rpos = ChunkRegion::getRegionPosition(neighborPos);

		// Combine the last bits of x, y, z into a 3-bit index (0-7)
		const int idx = (rpos.x & 1) | ((rpos.y & 1) << 1) | ((rpos.z & 1) << 2);

		// If this region bit-pattern hasn't been cached yet, fetch it
		if (!(cacheMask & (1 << idx)))
		{
			regionCache[idx] = Chunk::managerInstances->chunkRegion.getRegion(rpos);
			cacheMask |= (1 << idx);
		}

		if (regionCache[idx])
		{
			neighbors[i] = regionCache[idx]->chunks[ChunkRegion::getChunkIndexInRegion(neighborPos)];
		}
	}
}
