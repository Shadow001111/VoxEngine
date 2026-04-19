#include "WorldChunkManager.h"

#include "Game/TracyProfiler.h"
#include "Core/Multithreading/ThreadPool.h"

#include "Game/ProfileCategories.h"

#include "../World/ChunkLoaders/SphericalChunkLoader.h"

#include "../World/ChunkRegionManager.h"

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

	TRACY_SCOPE("Load chunks arond player", ProfileCategory::ChunkLoadUnload);

	// Update chunk loaders
	for (auto& chunkLoader : chunkLoaders)
	{
		chunkLoader->update(chunkLoaderPos, chunkLoadingDistance);
	}

	// Load chunks
	{
		TRACY_SCOPE("Load", ProfileCategory::ChunkLoadUnload);
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
		TRACY_SCOPE("Unload", ProfileCategory::ChunkLoadUnload);
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
	bool buildBlocks = false;
	{
		std::lock_guard<std::mutex> lock(buildContainers.blocksMutex);
		buildBlocks = !buildContainers.blocks.empty();
	}
	if (buildBlocks)
	{
		startBuildingChunkBlocks();
	}

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
	bool buildLights = false;
	{
		std::lock_guard<std::mutex> lock(buildContainers.lightsMutex);
		buildLights = !buildContainers.lights.empty();
	}
	if (buildLights)
	{
		startBuildingChunkLights();
	}

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
		TRACY_SCOPE("Check dirty meshes to send to GPU", ProfileCategory::ChunkMesh);

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
	// Collect chunks
	chunksToProcess.clear();
	{
		TRACY_SCOPE("Collect chunks for block building", ProfileCategory::ChunkBlocks);

		std::lock_guard<std::mutex> lock(buildContainers.blocksMutex);
		chunksToProcess.reserve(buildContainers.blocks.size());
		for (Chunk* chunk : buildContainers.blocks)
		{
			if (chunk->getState() == Chunk::State::NotInitialized_NeedsBlocks)
			{
				chunksToProcess.push_back(chunk);
			}
		}
		buildContainers.blocks.clear();
	}

	// Submit chunks to thread pool
	if (!chunksToProcess.empty())
	{
		TRACY_SCOPE("Send chunks to block building", ProfileCategory::ChunkBlocks);

		ThreadPool& pool = ParallelUtils::getGlobalThreadPool();

		const size_t chunkCount = chunksToProcess.size();
		for (size_t i = 0; i < chunkCount; i += CHUNKS_PER_BATCH)
		{
			size_t batchEnd = std::min(i + CHUNKS_PER_BATCH, chunkCount);
			std::vector<Chunk*> batch(chunksToProcess.begin() + i, chunksToProcess.begin() + batchEnd);

			pool.enqueue([this, batch_ = std::move(batch)]()
				{
					for (Chunk* chunk : batch_)
					{
						chunk->setState(Chunk::State::BuildingBlocks);
						chunk->buildBlocks();
					}

					{
						std::lock_guard<std::mutex> lock(buildContainers.lightsMutex);
						buildContainers.lights.insert(batch_.begin(), batch_.end());
					}
				});
		}
	}
	chunksToProcess.clear();
}

void WorldChunkManager::startBuildingChunkLights()
{
	// Collect chunks that are ready for light building
	chunksToProcess.clear();
	{
		TRACY_SCOPE("Collect chunks for light building", ProfileCategory::ChunkLight);

		std::lock_guard<std::mutex> lock(buildContainers.lightsMutex);
		const size_t chunkCount = buildContainers.lights.size();

		auto& remainingChunks = buildContainers.remainingLights;
		remainingChunks.reserve(chunkCount);

		chunksToProcess.reserve(chunkCount);
		for (Chunk* chunk : buildContainers.lights)
		{
			if (!chunk->areBlocksBuilt() || chunk->isLightBuilt())
			{
				continue;
			}

			// Check if all neighbors have blocks built
			bool allNeighborsReady = true;
			const auto& neigbors = chunk->getNeighbors();
			for (int i = 0; i < 6; i++)
			{
				const Chunk* neighbor = neigbors[Chunk::getSideNeighborIndex(i)];
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
				remainingChunks.insert(chunk);
			}
		}

		buildContainers.lights.swap(remainingChunks);
		remainingChunks.clear();
	}

	// Submit chunks to thread pool
	if (!chunksToProcess.empty())
	{
		TRACY_SCOPE("Send chunks to light building", ProfileCategory::ChunkLight);

		ThreadPool& pool = ParallelUtils::getGlobalThreadPool();

		const size_t chunkCount = chunksToProcess.size();
		const auto begin = chunksToProcess.cbegin();
		for (size_t i = 0; i < chunkCount; i += CHUNKS_PER_BATCH)
		{
			size_t batchEnd = std::min(i + CHUNKS_PER_BATCH, chunkCount);
			std::vector<Chunk*> batch(begin + i, begin + batchEnd);

			pool.enqueue([batch_ = std::move(batch)]()
				{
					for (Chunk* chunk : batch_)
					{
						chunk->buildLight();
					}
				});
		}
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

	TRACY_SCOPE("Collect chunks for light update", ProfileCategory::ChunkLight);

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
		TRACY_SCOPE("Collect chunks for mesh updating", ProfileCategory::ChunkMesh);

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
		TRACY_SCOPE("Collect chunks for connectivity updating", ProfileCategory::General);

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
	glm::ivec3 regionPosition;
	ChunkRegion* region;
	size_t index;

	{
		TRACY_SCOPE("Get region", ProfileCategory::ChunkLoadUnload);

		regionPosition = ChunkRegion::getRegionPosition(chunkPosition);
		region = Chunk::managerInstances->chunkRegion.getOrCreateRegion(regionPosition);
		index = ChunkRegion::getChunkIndexInRegion(chunkPosition);
	}

	// Check if chunk already exists
	if (region->chunks[index])
	{
		// If loaded, it should just add loader to existent chunk, but we don't need it right now.
		return;
	}

	// Find existing neighbors
	constexpr int selfIndex = Chunk::getNeighborIndex(0, 0, 0);
	std::array<Chunk*, 27> neighbors{ nullptr };
	{
		TRACY_SCOPE("Collect chunk neighbors", ProfileCategory::ChunkLoadUnload);
		for (int i = 0; i < neighbors.size(); i++)
		{
			if (i == selfIndex)
			{
				continue;
			}

			glm::ivec3 neighborOffset = Chunk::getNeighborOffset(i);
			glm::ivec3 neighborPosition = chunkPosition + neighborOffset;
			neighbors[i] = getChunkAt(neighborPosition);
		}
	}

	// Create and initialize chunk
	Chunk* chunk;
	{
		TRACY_SCOPE("Acuire chunk from pool", ProfileCategory::ChunkLoadUnload);
		chunk = chunkPool.acquire();
	}
	chunk->addLoader();
	chunk->init(chunkPosition, neighbors, region);

	// Send chunk to building blocks
	{
		TRACY_SCOPE("Insert in build block container", ProfileCategory::ChunkLoadUnload);

		std::lock_guard<std::mutex> lock(buildContainers.blocksMutex);
		buildContainers.blocks.insert(chunk);
	}

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
	if (!chunkRegion)
	{
		return;
	}

	// Get chunk index in region
	size_t index = ChunkRegion::getChunkIndexInRegion(chunkPosition);

	// Check if chunk exists
	Chunk* chunk = chunkRegion->chunks[index];
	if (!chunk)
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
