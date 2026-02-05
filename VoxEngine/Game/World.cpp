#include "World.h"

#include "World/Chunk/TerrainGenerator.h"
#include "World/Chunk/ChunkMeshManager.h"

#include "DataPackManagment/DataPackManager.h"
#include "DataPackManagment/AssetRegistry.h"

#include "Core/Profiler.h"
#include "Core/Multithreading/ThreadPool.h"

#include "SoundManager.h"

#include <stdexcept>
#include <glm/glm.hpp>

World::World() :
	// Passing references to sub-systems
	renderer({
			chunkManager.getAllChunks(),
			dayNightCycleValue,
			skyLightSub,
			appTime
	})
{
	// Datapack loading and registering assets
	std::vector<std::string> blockTextureNames;
	{
		PROFILE_SCOPE("Data packs loading", ProfileCategory::General);
		DataPackManager::loadAllDataPacks();
		blockTextureNames = AssetRegistry::getBlockTextureNames();
	}

	// Init WorldRenderer
	renderer.init(blockTextureNames);

	// Terrain generator
	{
		auto futures = ParallelUtils::getGlobalThreadPool().broadcast([]()
			{
				TerrainGenerator::initThread();
			});
		for (auto& future : futures)
		{
			future.wait();
		}
	}

	// Chunks
	Chunk::globalInit();

	// Entities
	Entity::world = this;

	// World directory
	try
	{
		const std::string worldName = "Test1";
		std::filesystem::path chunkSavesPath = std::filesystem::path("Worlds") / worldName / "Chunks";
		if (!std::filesystem::exists(chunkSavesPath))
		{
			if (!std::filesystem::create_directories(chunkSavesPath))
			{
				throw std::runtime_error("Failed to create world directory: " + chunkSavesPath.string());
			}
		}

		Chunk::CHUNK_SAVES_PATH = chunkSavesPath;
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		throw std::runtime_error("Filesystem error creating world: " + std::string(e.what()));
	}
}

void World::preparation()
{
	int area = 0;
	{
		int P = 0;
		int rsq = chunkLoadingDistance * chunkLoadingDistance;
		for (int x = 1; x < chunkLoadingDistance; x++)
		{
			P += (int)sqrtf(float(rsq - x * x));
		}
		area = (P + chunkLoadingDistance) * 4 + 1;
	}
	size_t chunkCount = 0;
	{
		int P = 0;
		int rsq = chunkLoadingDistance * chunkLoadingDistance;
		for (int x = 1; x < chunkLoadingDistance; x++)
		{
			int D1 = rsq - x * x;
			int maxY = (int)sqrt(D1);
			for (int y = 1; y <= maxY; y++)
			{
				P += (int)sqrtf(float(D1 - y * y));
			}
		}
		chunkCount = P * 8 + (area - chunkLoadingDistance * 2) * 3 - 2;
	}

	chunkManager.preparation(chunkCount);

	/*size_t maxFacesCount = chunkCount * size_t(CHUNK_VOLUME * 6);
	ChunkMeshManager::getInstance().preallocateMemory(maxFacesCount);

	chunkDrawCommandBuffer->allocateMemory(chunkCount * sizeof(DrawArraysIndirectCommand));
	chunkPositionSSBO->allocateMemory(chunkCount * sizeof(glm::vec3));*/
}

void World::loadChunks(const glm::dvec3& playerPos)
{
	chunkManager.loadChunks(playerPos, chunkLoadingDistance);
}

void World::update(float deltaTime)
{
	// Update time
	worldTime = (worldTime + 1) % TICKS_PER_24_HOURS;
	{
		const float t = (float)worldTime / (float)TICKS_PER_24_HOURS;
		const float cosValue = cosf(t * 2.0f * 3.14159f);
		dayNightCycleValue = (cosValue + 1.0f) * 0.5f;
		skyLightSub = (1.0f - dayNightCycleValue) * 15.0f;
	}

	// Update chunk manager
	chunkManager.update();

	// Update renderer
	renderer.update();

	// Update entities
	for (auto& pair : entities)
	{
		auto& entity = pair.second;
		entity->update(deltaTime);
	}
}

void World::sendChunkMeshesToGPU()
{
	chunkManager.sendChunkMeshesToGPU();
}

void World::render(const Camera& camera, const FrameBuffer& FBO, const RaycastResult& raycast)
{
	renderer.render(camera, FBO, raycast);
}

// TODO: Make raycast undependable of float precision. Or do the same for voxel marker rendering.
RaycastResult World::raycast(const glm::dvec3& origin, const glm::dvec3& direction, float maxDistance) const
{
	PROFILE_SCOPE("Raycast", ProfileCategory::General);

	RaycastResult result;

	const glm::dvec3 dir = glm::normalize(direction);

	// DDA algorithm for voxel traversal
	glm::ivec3 blockPos = glm::ivec3(glm::floor(origin));

	// Step direction for each axis
	const glm::ivec3 step = glm::sign(dir);

	// tMax: distance to next voxel boundary along each axis
	// tDelta: distance to move along ray to cross one voxel on each axis
	glm::dvec3 tDelta, tMax;

	for (int i = 0; i < 3; i++)
	{
		if (std::abs(dir[i]) < 0.0001)
		{
			tDelta[i] = std::numeric_limits<double>::max();
			tMax[i] = std::numeric_limits<double>::max();
		}
		else
		{
			double invDir = 1.0 / dir[i];
			double delta = abs(invDir);
			tDelta[i] = delta;
			if (step[i] > 0)
			{
				tMax[i] = (1.0 - glm::fract(origin[i])) * delta;
			}
			else
			{
				tMax[i] = glm::fract(origin[i]) * delta;
			}
		}
	}

	double distanceTraveled = 0.0;
	int lastAxis = -1;

	// Cache chunk
	Chunk* chunk = nullptr;
	glm::ivec3 cachedChunkPos = glm::ivec3(std::numeric_limits<int>::max());

	// Traverse voxels
	while (distanceTraveled < maxDistance)
	{
		// Get and cache current chunk
		glm::ivec3 chunkPos = blockPos >> CHUNK_SIZE_LOG2;

		if (chunkPos != cachedChunkPos)
		{
			cachedChunkPos = chunkPos;
			chunk = chunkManager.getChunkAt(cachedChunkPos);
		}

		// Check current block
		if (chunk && chunk->getState() > Chunk::State::BuildingBlocks)
		{
			// Local block position within chunk
			glm::ivec3 localBlockPos = blockPos & CHUNK_LOWER_BITS_MASK;

			BlockId block = chunk->getBlockAt(localBlockPos.x, localBlockPos.y, localBlockPos.z);
			const BlockData* blockData = AssetRegistry::getBlockData(block);
			if (blockData && blockData->raycastable)
			{
				result.hit = true;
				result.hitBlock = block;
				result.hitPosition = origin + dir * (double)distanceTraveled;
				result.hitBlockPosition = blockPos;
				result.hitChunk = chunk;
				if (lastAxis == -1)
				{
					result.hitNormal = -1;
				}
				else
				{
					result.hitNormal = lastAxis * 2 + (step[lastAxis] > 0 ? 0 : 1);
				}
				result.distance = (float)distanceTraveled;
				return result;
			}
		}

		// Find which axis boundary we hit first
		if (tMax.x < tMax.y)
		{
			if (tMax.x < tMax.z)
			{
				distanceTraveled = tMax.x;
				tMax.x += tDelta.x;
				blockPos.x += step.x;
				lastAxis = 0;
			}
			else
			{
				distanceTraveled = tMax.z;
				tMax.z += tDelta.z;
				blockPos.z += step.z;
				lastAxis = 2;
			}
		}
		else
		{
			if (tMax.y < tMax.z)
			{
				distanceTraveled = tMax.y;
				tMax.y += tDelta.y;
				blockPos.y += step.y;
				lastAxis = 1;
			}
			else
			{
				distanceTraveled = tMax.z;
				tMax.z += tDelta.z;
				blockPos.z += step.z;
				lastAxis = 2;
			}
		}
	}

	return result; // No hit
}

void World::rebuildAllChunkMeshes()
{
	chunkManager.rebuildAllChunkMeshes();
}

void World::debugMethod()
{
	
}

const World::DebugData& World::getDebugData(bool updateIntense) const
{
	PROFILE_SCOPE("World debug data collection", ProfileCategory::General);

	const auto& chunks = chunkManager.getAllChunks();

	// Chunks count
	debugData.loadedChunksCount = chunks.size();

	// Total chunk block face count
	if (updateIntense)
	{
		debugData.totalChunkFaceCount = 0;
		for (const auto& pair : chunks)
		{
			const Chunk* chunk = pair.second;

			debugData.totalChunkFaceCount += chunk->getFaceCount();
		}
	}

	// Chunk face capacity
	const size_t alignedCapacity = ChunkMeshManager::getInstance().getAlignedInstanceVBO().getCapacity();
	const size_t nonAlignedCapacity = ChunkMeshManager::getInstance().getNonAlignedInstanceVBO().getCapacity();

	debugData.totalChunkFaceCapacity = alignedCapacity / sizeof(AlignedBlockFace) + nonAlignedCapacity / sizeof(NonAlignedBlockFace);

	debugData.totalChunkFaceCapacityInBytes = alignedCapacity + nonAlignedCapacity;

	// Render stats
	debugData.renderStats = renderer.getRenderStats();

	return debugData;
}

bool World::placeBlock(const RaycastResult& raycast, BlockId block)
{
	// TODO: Don't place block if entity is there
	if (!raycast.hit)
	{
		return false;
	}

	// Calculate placement position based on hit normal
	glm::ivec3 placePos = raycast.hitBlockPosition;

	switch (raycast.hitNormal)
	{
	case 0: placePos.x--; break; // -X
	case 1: placePos.x++; break; // +X
	case 2: placePos.y--; break; // -Y
	case 3: placePos.y++; break; // +Y
	case 4: placePos.z--; break; // -Z
	case 5: placePos.z++; break; // +Z
	}

	updateBlockAt(placePos, block);

	const BlockData* blockData = AssetRegistry::getBlockData(block);
	if (blockData && !blockData->placeSounds.empty())
	{
		// Choose random sounds from vector
		const auto& sounds = blockData->placeSounds;
		int soundIndex = rand() % sounds.size();
		auto& sndMgr = SoundManager::getInstance();
		sndMgr.play("block/place/" + sounds[soundIndex]);
	}

	return true;
}

bool World::breakBlock(const RaycastResult& raycast)
{
	if (!raycast.hit)
	{
		return false;
	}

	updateBlockAt(raycast.hitBlockPosition, AssetRegistry::getBlockNumericalId("core:air"));

	const BlockData* blockData = AssetRegistry::getBlockData(raycast.hitBlock);
	if (blockData && !blockData->breakSounds.empty())
	{
		// Choose random sounds from vector
		const auto& sounds = blockData->breakSounds;
		int soundIndex = rand() % sounds.size();
		auto& sndMgr = SoundManager::getInstance();
		sndMgr.play("block/break/" + sounds[soundIndex]);
	}

	return true;
}

void World::updateBlockAt(const glm::ivec3& worldPos, BlockId block)
{
	// Convert world position to chunk position and local position
	glm::ivec3 chunkPos = worldPos >> CHUNK_SIZE_LOG2;
	glm::ivec3 localPos = worldPos & CHUNK_LOWER_BITS_MASK;

	// Get the chunk
	Chunk* chunk = chunkManager.getChunkAt(chunkPos);
	if (!chunk)
	{
		return;
	}

	// Update the block
	// Note: Can possibly break something if chunk is in the middle of processing
	chunk->setBlockAt(localPos.x, localPos.y, localPos.z, block);
}

std::optional<BlockId> World::getBlockAt(const glm::ivec3& globalPosition) const
{
	glm::ivec3 chunkPos = globalPosition >> CHUNK_SIZE_LOG2;

	const Chunk* chunk = chunkManager.getChunkAt(chunkPos);
	if (!chunk)
	{
		return std::nullopt;
	}

	glm::ivec3 localBlockPos = globalPosition & CHUNK_LOWER_BITS_MASK;
	return chunk->getBlockAt(localBlockPos.x, localBlockPos.y, localBlockPos.z);
}

void World::setChunkLoadingDistance(int loadingDistanceInChunks)
{
	chunkLoadingDistance = loadingDistanceInChunks;
	renderer.setRenderDistance(loadingDistanceInChunks);
}

void World::setAppTime(float time)
{
	this->appTime = time;
}