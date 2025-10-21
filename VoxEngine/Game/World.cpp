#include "World.h"

#include "World/Chunk/TerrainGenerator.h"
#include "World/Chunk/ChunkMeshManager.h"

#include "Core/Profiler.h"
#include "Core/Multithreading/ThreadPool.h"

#include <iostream>

World::World()
{
	// Shaders
	{
		std::vector<Shader::ShaderSource> faceShaderSources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/face.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/face.frag"}
		};
		faceShader = std::make_unique<Shader>(faceShaderSources);
		faceShader->use();
		faceShader->setFloat("CHUNK_SIZE", CHUNK_SIZE);

		std::vector<Shader::ShaderSource> voxelMarkerShaderSources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/voxelMarker.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/voxelMarker.frag"}
		};
		voxelMarkerShader = std::make_unique<Shader>(voxelMarkerShaderSources);
	}

	// Block data base
	BlockDataBase::loadBlockDataBase();

	// Block textures
	std::vector<std::string> blockTextureNames;
	{
		PROFILE_SCOPE("BlockTextureIDDatabase build", ProfileCategory::General);
		Chunk::blockTextureDatabase.build(blockTextureNames);
	}
	{
		PROFILE_SCOPE("Block texture array creation", ProfileCategory::General);
		blockTextureArray = std::make_unique<BlockTextureArray>("res/Textures", blockTextureNames, 0, 16);
	}
	blockTextureArray->bind();
	faceShader->use();
	faceShader->setInt("blockTextures", blockTextureArray->getUnit());

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

	// Chunk draw command buffer
	chunkDrawCommandBuffer = std::make_unique<OpenGL_Buffer>(GL_DRAW_INDIRECT_BUFFER, GL_DYNAMIC_DRAW);

	// Chunk position SSBO
	chunkPositionSSBO = std::make_unique<OpenGL_SSBO>(0);
	chunkPositionSSBO->bindBase();
}

World::~World()
{
}

void World::preparation(int renderDistance)
{
	if (renderDistance <= 0)
	{
		return;
	}

	int area = 0;
	{
		int P = 0;
		int rsq = renderDistance * renderDistance;
		for (int x = 1; x < renderDistance; x++)
		{
			P += (int)sqrtf(rsq - x * x);
		}
		area = (P + renderDistance) * 4 + 1;
	}
	size_t chunkCount = 0;
	{
		int P = 0;
		int rsq = renderDistance * renderDistance;
		for (int x = 1; x < renderDistance; x++)
		{
			int D1 = rsq - x * x;
			int maxY = (int)sqrt(D1);
			for (int y = 1; y <= maxY; y++)
			{
				P += (int)sqrtf(D1 - y * y);
			}
		}
		chunkCount = P * 8 + (area - renderDistance * 2) * 3 - 2;
	}

	chunkPool.allocate(chunkCount + 10);
	chunks.reserve(chunkCount + 10);

	size_t maxFacesCount = (size_t)chunkCount * (CHUNK_VOLUME + CHUNK_AREA) * 3;
	ChunkMeshManager::getInstance().preallocateMemory(maxFacesCount);

	chunkDrawCommandBuffer->allocateMemory(chunkCount * sizeof(DrawArraysIndirectCommand));
	chunkPositionSSBO->allocateMemory(chunkCount * sizeof(glm::vec3));
}

void World::loadChunksAroundPlayer(const glm::vec3& loaderPos, int renderDistance)
{
	glm::ivec3 chunkLoaderPos = glm::ivec3(glm::floor(loaderPos)) >> CHUNK_SIZE_LOG2;
	if (lastChunkLoaderPos == chunkLoaderPos)
    {
        return;
    }
	lastChunkLoaderPos = chunkLoaderPos;

	// Unload chunks that are out of range
	unloadChunksOutsideRange(renderDistance);

	// Load chunks in a spherical area around the lastChunkLoaderPos

	static std::vector<Chunk*> chunksToSend;
	chunksToSend.clear();

	{
		PROFILE_SCOPE("Load chunks", ProfileCategory::ChunkLoadUnload);

		int renderDistanceSquared = renderDistance * renderDistance;
		for (int x = -renderDistance; x <= renderDistance; x++)
		{
			int chunkX = chunkLoaderPos.x + x;

			int D1 = renderDistanceSquared - x * x;
			int yRange = (int)sqrtf(D1);

			for (int y = -yRange; y <= yRange; y++)
			{
				int chunkY = chunkLoaderPos.y + y;

				int D2 = D1 - y * y;
				int zRange = (int)sqrtf(D2);

				for (int z = -zRange; z <= zRange; z++)
				{
					int chunkZ = chunkLoaderPos.z + z;

					loadChunk(chunkX, chunkY, chunkZ, chunksToSend);
				}
			}
		}
	}

	{
		// TODO: Maybe use vectors instead of unordered sets? Then I could use std::vector.insert to insert the whole vector.
		std::lock_guard<std::mutex> lock(buildBlocksMutex);
		for (Chunk* chunkPtr : chunksToSend)
		{
			buildBlocksContainer.insert(chunkPtr);
		}
	}
}

void World::update()
{
	chunkPool.returnProcessingChunksToPool();

	if (!buildBlocksContainer.empty())
	{
		startBuildingChunkBlocks();
	}

	if (!buildLightContainer.empty())
	{
		startBuildingChunkLights();
	}

	collectChunksNeedingLightUpdate();
	if (!lightUpdateContainer.empty())
	{
		updateChunkLights();
	}

	if (!buildMeshContainer.empty())
	{
		startBuildingChunkMeshes();
	}
}

void World::sortChunkMeshes(const glm::vec3& cameraPos)
{
	// No multithreading for now
	const glm::ivec3 cameraBlockPos = glm::floor(cameraPos);
	const bool forceToSort = cameraBlockPos != lastChunkMeshSortPos;
	lastChunkMeshSortPos = cameraBlockPos;

	for (auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();
		if (forceToSort || chunk->shouldMeshBeSorted())
		{
			chunk->sortMesh(lastChunkMeshSortPos);
		}
	}
}

void World::sendChunkMeshesToGPU()
{
	// Sends only dirty meshes
	for (auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();
		chunk->askForMeshUpload();
	}
	Chunk::sendMeshesToGPU();
}

void World::renderChunks(const Camera& camera) const
{
	faceShader->use();
	{
		// Matrices
		faceShader->setMat4("view", camera.getViewMatrix());
		faceShader->setMat4("projection", camera.getProjectionMatrix());

		// Fog
		const auto& fogColor = visuals.backgroundColor;
		faceShader->setVec3("fogColor", fogColor.r, fogColor.g, fogColor.b);
		faceShader->setFloat("fogDensity", visuals.fogDensity);
		faceShader->setFloat("fogGradient", visuals.fogGradient);
	}

	// Collect chunks to render
	std::vector<ChunkRenderInfo> chunksToRender;
	{
		PROFILE_SCOPE("Render: collect chunks", ProfileCategory::Render);
		collectChunksToRender(chunksToRender, camera);
	}

	// Sort chunks by distance
	{
		PROFILE_SCOPE("Render: sort chunks", ProfileCategory::Render);
		std::sort(chunksToRender.begin(), chunksToRender.end(),
			[](const ChunkRenderInfo& a, const ChunkRenderInfo& b)
			{
				return a.manhattanDistance < b.manhattanDistance;
			});
	}

	// Bind resources
	ChunkMeshManager::getInstance().bindVAO();
	blockTextureArray->bind();
	chunkDrawCommandBuffer->bind();
	chunkPositionSSBO->bind();

	// Set shared opengl states
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	// Debug data
	debugData.renderedChunks = chunksToRender.size();
	debugData.renderedFaceCount = 0;

	// Opaque
	std::vector<DrawArraysIndirectCommand> chunkDrawCommands;
	std::vector<glm::ivec3> chunkPositions;
	{
		{
			PROFILE_SCOPE("Render: collect draw commands", ProfileCategory::Render);

			for (const auto& info : chunksToRender)
			{
				info.chunk->collectOpaqueRenderData(chunkDrawCommands, chunkPositions);
			}
		}

		const size_t drawCount = chunkDrawCommands.size();
		if (drawCount > 0)
		{
			debugData.renderedFaceCount += drawCount;

			glDisable(GL_BLEND);

			chunkDrawCommandBuffer->allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer->write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO->allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO->write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));
		
			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, drawCount, 0);
		}
	}

	// Transparent
	std::reverse(chunksToRender.begin(), chunksToRender.end());
	chunkDrawCommands.clear();
	chunkPositions.clear();
	{
		{
			PROFILE_SCOPE("Render: collect draw commands", ProfileCategory::Render);

			for (const auto& info : chunksToRender)
			{
				info.chunk->collectTransparentRenderData(chunkDrawCommands, chunkPositions);
			}
		}

		const size_t drawCount = chunkDrawCommands.size();
		if (drawCount > 0)
		{
			debugData.renderedFaceCount += drawCount;

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			chunkDrawCommandBuffer->allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer->write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO->allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO->write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));

			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, drawCount, 0);
		}
	}
}

void World::renderVoxelMarker(const Camera& camera, const RaycastResult& raycast) const
{
	if (!raycast.hit)
	{
		return;
	}

	voxelMarkerShader->use();
	{
		// Matrices
		voxelMarkerShader->setMat4("view", camera.getViewMatrix());
		voxelMarkerShader->setMat4("projection", camera.getProjectionMatrix());
	}

	auto placePos = raycast.hitBlockPosition;
	
	/*switch (raycast.hitNormal)
	{
	case 0:
		placePos.x--;
		break;
	case 1:
		placePos.x++;
		break;
	case 2:
		placePos.y--;
		break;
	case 3:
		placePos.y++;
		break;
	case 4:
		placePos.z--;
		break;
	case 5:
		placePos.z++;
		break;
	}*/

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);

	{
		const auto& pos = placePos;
		voxelMarkerShader->setVec3("position", pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f);
		voxelMarkerShader->setFloat("scale", 1.01f);
		voxelMarkerShader->setVec3("color", 1.0f, 0.0f, 1.0f);
		voxelMarkerMesh.draw();
	}
	{
		const auto& pos = raycast.hitPosition;
		voxelMarkerShader->setVec3("position", pos.x, pos.y, pos.z);
		voxelMarkerShader->setFloat("scale", 0.2f);
		voxelMarkerShader->setVec3("color", 1.0f, 0.0f, 0.0f);
		voxelMarkerMesh.draw();
	}
}

World::RaycastResult World::raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const
{
	PROFILE_SCOPE("Raycast", ProfileCategory::General);

	RaycastResult result;

	const glm::vec3 dir = glm::normalize(direction);

	// DDA algorithm for voxel traversal
	glm::vec3 currentPos = origin;
	glm::ivec3 blockPos = glm::ivec3(glm::floor(currentPos));

	// Step direction for each axis
	const glm::ivec3 step = glm::sign(dir);

	// tMax: distance to next voxel boundary along each axis
	// tDelta: distance to move along ray to cross one voxel on each axis
	glm::vec3 tDelta, tMax;

	for (int i = 0; i < 3; i++)
	{
		if (std::abs(dir[i]) < 0.0001f)
		{
			tDelta[i] = std::numeric_limits<float>::max();
			tMax[i] = std::numeric_limits<float>::max();
		}
		else
		{
			float invDir = 1.0f / dir[i];
			float delta = fabsf(invDir);
			tDelta[i] = delta;
			if (step[i] > 0)
			{
				tMax[i] = (1.0f - glm::fract(currentPos[i])) * delta;
			}
			else
			{
				tMax[i] = glm::fract(currentPos[i]) * delta;
			}
		}
	}

	float distanceTraveled = 0.0f;
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
			chunk = getChunkAt(cachedChunkPos);
		}

		// Check current block
		if (chunk && chunk->getState() > Chunk::State::BuildingBlocks)
		{
			// Local block position within chunk
			glm::ivec3 localBlockPos = blockPos & CHUNK_LOWER_BITS_MASK;

			Block block = chunk->getBlock_inBoundaries(localBlockPos.x, localBlockPos.y, localBlockPos.z);
			const BlockData* blockData = BlockDataBase::getBlockData(block);

			if (blockData->properties.raycastable)
			{
				result.hit = true;
				result.hitBlock = block;
				result.hitPosition = origin + dir * distanceTraveled;
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
				result.distance = distanceTraveled;
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
	// Queue all ready chunks for mesh rebuild
	{
		std::lock_guard<std::mutex> lock(buildMeshMutex);
		buildMeshContainer.clear();
		for (const auto& pair : chunks)
		{
			Chunk* chunk = pair.second.get();
			if (chunk->getState() == Chunk::State::Ready)
			{
				chunk->setState(Chunk::State::NeedsMesh);
				buildMeshContainer.insert(chunk);
			}
		}
	}
}

void World::debugMethod()
{
	
}

const World::DebugData& World::getDebugData() const
{
	debugData.totalFaces = 0;
	for (const auto& pair : chunks)
	{
		const Chunk* chunk = pair.second.get();

		debugData.totalFaces += chunk->getFaceCount();
	}
	debugData.totalFaceCapacity = ChunkMeshManager::getInstance().getInstanceVBO().getCapacity() / sizeof(BlockFaceInstance);

	debugData.loadedChunksCount = chunks.size();

	debugData.chunkMeshesGaps = ChunkMeshManager::getInstance().getGaps();

	debugData.chunkDrawCommandBufferSizeInBytes = chunkDrawCommandBuffer->getCapacity();
	debugData.chunkPositionBufferSizeInBytes = chunkPositionSSBO->getCapacity();

	return debugData;
}

Chunk* World::getChunkAt(const glm::ivec3& position) const
{
	auto it = chunks.find(position);
	if (it == chunks.end())
	{
		return nullptr;
	}
	return it->second.get();
}

bool World::chunkExistsAt(const glm::ivec3& position) const
{
	return chunks.find(position) != chunks.end();
}

void World::unloadChunksOutsideRange(int renderDistance)
{
	std::vector<glm::ivec3> chunksToUnload;
	{
		PROFILE_SCOPE("Unload chunks: collect", ProfileCategory::ChunkLoadUnload);

		const int x = lastChunkLoaderPos.x;
		const int y = lastChunkLoaderPos.y;
		const int z = lastChunkLoaderPos.z;

		int renderDistanceSquared = renderDistance * renderDistance;

		for (const auto& pair : chunks)
		{
			const glm::ivec3& pos = pair.first;
			
			// Calculate squared distance from chunk loader position
			int dx = pos.x - x;
			int dy = pos.y - y;
			int dz = pos.z - z;
			int distanceSquared = dx * dx + dy * dy + dz * dz;
			
			// Unload if outside spherical range
			if (distanceSquared > renderDistanceSquared)
			{
				chunksToUnload.push_back(pos);
			}
		}
	}
	{
		PROFILE_SCOPE("Unload chunks: unload", ProfileCategory::ChunkLoadUnload);
		for (const glm::ivec3& pos : chunksToUnload)
		{
			auto it = chunks.find(pos);

			Chunk* chunk = it->second.get();
			chunkPool.release(std::move(it->second));
			chunks.erase(it);
		}
	}
}

void World::loadChunk(int chunkX, int chunkY, int chunkZ, std::vector<Chunk*>& chunksToSend)
{
	// Check if chunk already exists
	glm::ivec3 chunkPosition = { chunkX, chunkY, chunkZ };
	if (chunkExistsAt(chunkPosition))
	{
		return;
	}

	// Find existing neighbors
	Chunk* neighbors[6] = {
		getChunkAt({ chunkX - 1, chunkY, chunkZ }),
		getChunkAt({ chunkX + 1, chunkY, chunkZ }),
		getChunkAt({ chunkX, chunkY - 1, chunkZ }),
		getChunkAt({ chunkX, chunkY + 1, chunkZ }),
		getChunkAt({ chunkX, chunkY, chunkZ - 1 }),
		getChunkAt({ chunkX, chunkY, chunkZ + 1 })
	};

	// Create and initialize chunk
	std::unique_ptr<Chunk> chunk = chunkPool.acquire();
	chunk->init(chunkX, chunkY, chunkZ, neighbors);
	chunksToSend.push_back(chunk.get());
	chunks.emplace(chunkPosition, std::move(chunk));
}

void World::startBuildingChunkBlocks()
{
	// Collect chunks
	std::vector<Chunk*> chunksToProcess;
	{
		PROFILE_SCOPE("Collect chunks for block building", ProfileCategory::ChunkBlocks);

		std::lock_guard<std::mutex> lock(buildBlocksMutex);
		if (buildBlocksContainer.empty())
		{
			return;
		}

		chunksToProcess.reserve(buildBlocksContainer.size());
		for (Chunk* chunk : buildBlocksContainer)
		{
			chunk->setState(Chunk::State::BuildingBlocks);
			chunksToProcess.push_back(chunk);
		}
		buildBlocksContainer.clear();
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
						std::lock_guard<std::mutex> lock(buildLightMutex);
						buildLightContainer.insert(chunk);
					}

					/*for (int i = 0; i < 6; i++)
					{
						Chunk* neighbor = chunk->neighbors[i];
						if (neighbor && neighbor->getState() == Chunk::State::NeedsLight)
						{
							std::lock_guard<std::mutex> lock(buildLightMutex);
							buildLightContainer.insert(neighbor);
						}
					}*/
				});
		}
	}
}

void World::startBuildingChunkLights()
{
	// Collect chunks that are ready for light building
	std::vector<Chunk*> chunksToProcess;
	{
		PROFILE_SCOPE("Collect chunks for light building", ProfileCategory::ChunkLight);

		std::lock_guard<std::mutex> lock(buildLightMutex);
		if (buildLightContainer.empty())
		{
			return;
		}

		std::unordered_set<Chunk*> remainingChunks;
		remainingChunks.reserve(buildLightContainer.size());

		chunksToProcess.reserve(buildLightContainer.size());
		for (Chunk* chunk : buildLightContainer)
		{
			if (chunk->getState() != Chunk::State::NeedsLight)
			{
				continue;
			}

			// Check if all neighbors have blocks built
			bool allNeighborsReady = true;
			for (int i = 0; i < 6; i++)
			{
				Chunk* neighbor = chunk->neighbors[i];
				if (neighbor)
				{
					Chunk::State neighborState = neighbor->getState();
					if (neighborState == Chunk::State::NeedsBlocks ||
						neighborState == Chunk::State::BuildingBlocks)
					{
						allNeighborsReady = false;
						break;
					}
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

		buildLightContainer.swap(remainingChunks);
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

					if (!chunk->getIsLoadedInWorld())
					{
						return;
					}

					chunk->setState(Chunk::State::NeedsMesh);

					// Queue for mesh building
					{
						std::lock_guard<std::mutex> lock(buildMeshMutex);
						buildMeshContainer.insert(chunk);

						// Also rebuild mesh for neighbors that might be affected
						for (int i = 0; i < 6; i++)
						{
							Chunk* neighbor = chunk->neighbors[i];
							if (neighbor && neighbor->getState() == Chunk::State::Ready)
							{
								buildMeshContainer.insert(neighbor);
							}
						}
					}
				});
		}
	}
}

void World::startBuildingChunkMeshes()
{
	// Collect chunks
	std::vector<Chunk*> chunksToProcess;
	{
		PROFILE_SCOPE("Collect chunks for mesh building", ProfileCategory::ChunkMesh);

		std::lock_guard<std::mutex> lock(buildMeshMutex);
		if (buildMeshContainer.empty())
		{
			return;
		}

		std::unordered_set<Chunk*> remainingChunks;
		remainingChunks.reserve(buildMeshContainer.size());

		chunksToProcess.reserve(buildMeshContainer.size());
		for (Chunk* chunk : buildMeshContainer)
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
		buildMeshContainer.swap(remainingChunks);
	}

	// Submit chunks to thread pool
	{
		PROFILE_SCOPE("Send chunks to mesh building", ProfileCategory::ChunkMesh);

		ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
		for (Chunk* chunk : chunksToProcess)
		{
			pool.enqueue([chunk]()
				{
					// Build mesh in background thread (no OpenGL calls here)
					chunk->buildMesh();

					if (!chunk->getIsLoadedInWorld())
					{
						return;
					}

					chunk->setState(Chunk::State::Ready);
				});
		}
	}
}

void World::updateChunkLights()
{
	std::unordered_set<Chunk*> chunksToUpdate;
	std::unordered_set<Chunk*> affectedChunks;

	chunksToUpdate.swap(lightUpdateContainer);

	// Process light updates (on main thread for now, could be threaded later)
	for (Chunk* chunk : chunksToUpdate)
	{
		if (!chunk->getIsLoadedInWorld())
		{
			continue;
		}

		chunk->updateLight();
		affectedChunks.insert(chunk);

		// Check if neighbors need updates too
		for (int i = 0; i < 6; i++)
		{
			Chunk* neighbor = chunk->neighbors[i];
			if (neighbor && neighbor->hasLightUpdates())
			{
				affectedChunks.insert(neighbor);
			}
		}
	}

	// Re-check for more updates (light can cascade)
	for (Chunk* chunk : affectedChunks)
	{
		if (chunk->hasLightUpdates())
		{
			lightUpdateContainer.insert(chunk);
		}
	}

	// Queue affected chunks for mesh rebuild
	{
		std::lock_guard<std::mutex> lock(buildMeshMutex);
		for (Chunk* chunk : affectedChunks)
		{
			if (chunk->getState() == Chunk::State::NeedsMesh)
			{
				buildMeshContainer.insert(chunk);

				// Also rebuild neighbors that might be affected by lighting changes
				for (int i = 0; i < 6; i++)
				{
					Chunk* neighbor = chunk->neighbors[i];
					if (neighbor && neighbor->getState() == Chunk::State::Ready)
					{
						neighbor->setState(Chunk::State::NeedsMesh);
						buildMeshContainer.insert(neighbor);
					}
				}
			}
		}
	}
}

void World::collectChunksNeedingLightUpdate()
{
	PROFILE_SCOPE("Collect chunks needing light update", ProfileCategory::ChunkLight);

	for (const auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();

		// Only update chunks that are ready and have pending light updates
		if (chunk->getState() == Chunk::State::Ready && chunk->hasLightUpdates())
		{
			lightUpdateContainer.insert(chunk);
		}
	}
}

void World::collectChunksToRender(std::vector<ChunkRenderInfo>& chunksToRender, const Camera& camera) const
{
	chunksToRender.reserve(chunks.size());

	const Frustum& frustum = camera.getFrustum();
	Box chunkShape(glm::vec3(0.0f), glm::vec3(CHUNK_SIZE * 0.5f));

	const glm::vec3 cameraPosition = camera.getPosition();
	const glm::ivec3 cameraChunkPosition = glm::ivec3(cameraPosition) >> CHUNK_SIZE_LOG2;

	for (const auto& pair : chunks)
	{
		const Chunk* chunk = pair.second.get();

		if (!chunk->canBeRendered())
		{
			continue;
		}

		// Check is chunk is on frustum
		glm::ivec3 chunkPosition = chunk->getPosition();
		glm::vec3 chunkWorldPosition = glm::vec3(chunkPosition * CHUNK_SIZE);

		chunkShape.center = chunkWorldPosition + chunkShape.halfExtents;
		if (!frustum.checkBox(chunkShape))
		{
			continue;
		}

		glm::ivec3 delta = glm::abs(chunkPosition - cameraChunkPosition);

		unsigned int manhattanDistance = delta.x + delta.y + delta.z;
		chunksToRender.emplace_back(chunk, manhattanDistance);
	}
}

//============================================================================
// ChunkRenderInfo

World::ChunkRenderInfo::ChunkRenderInfo(const Chunk* chunk, unsigned int manhattanDistance) :
	chunk(chunk), manhattanDistance(manhattanDistance)
{
}

//============================================================================