#include "World.h"

#include "World/Chunk/TerrainGenerator.h"
#include "World/Chunk/ChunkMeshManager.h"

#include "World/Chunk/BlockData.h"

#include "Core/Profiler.h"
#include "Core/Multithreading/ThreadPool.h"

World::World()
{
	// Visual settings
	visualSettings.backgroundColor = { 0.52f, 0.8f, 0.92f }; // Sky color
	visualSettings.fogGradient = 5.0f;

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
	std::vector<std::string> blockTextureNames;
	BlockDataBase::loadBlockDataBase(blockTextureNames);


	// Block textures
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

	// Entities
	Entity::world = this;
}

World::~World()
{
}

void World::preparation()
{
	int area = 0;
	{
		int P = 0;
		int rsq = chunkLoadingDistance * chunkLoadingDistance;
		for (int x = 1; x < chunkLoadingDistance; x++)
		{
			P += (int)sqrtf(rsq - x * x);
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
				P += (int)sqrtf(D1 - y * y);
			}
		}
		chunkCount = P * 8 + (area - chunkLoadingDistance * 2) * 3 - 2;
	}

	chunkPool.allocate(chunkCount + 10);
	chunks.reserve(chunkCount + 10);

	size_t maxFacesCount = chunkCount * (size_t)(CHUNK_VOLUME + CHUNK_AREA) * (size_t)3;
	ChunkMeshManager::getInstance().preallocateMemory(maxFacesCount);

	chunkDrawCommandBuffer->allocateMemory(chunkCount * sizeof(DrawArraysIndirectCommand));
	chunkPositionSSBO->allocateMemory(chunkCount * sizeof(glm::vec3));
}

void World::loadChunksAroundPlayer(const glm::vec3& loaderPos)
{
	glm::ivec3 chunkLoaderPos = glm::ivec3(glm::floor(loaderPos)) >> CHUNK_SIZE_LOG2;
	if (lastChunkLoaderPos == chunkLoaderPos)
    {
        return;
    }
	lastChunkLoaderPos = chunkLoaderPos;

	// Unload chunks that are out of range
	unloadChunksOutsideRange();

	// Load chunks in a spherical area around the lastChunkLoaderPos

	static std::vector<Chunk*> chunksToSend;
	chunksToSend.clear();

	{
		PROFILE_SCOPE("Load chunks", ProfileCategory::ChunkLoadUnload);

		int renderDistanceSquared = chunkLoadingDistance * chunkLoadingDistance;
		for (int x = -chunkLoadingDistance; x <= chunkLoadingDistance; x++)
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

void World::update(float deltaTime)
{
	// Chunks
	chunkPool.returnProcessingChunksToPool();

	if (!buildBlocksContainer.empty())
	{
		startBuildingChunkBlocks();
	}

	if (!buildLightContainer.empty())
	{
		startBuildingChunkLights();
	}

	// TODO: Sometimes loops forever? Because of sky light propagation.
	// If not limited, it goes forever. But when limited to 20 iterations, next fame iteration count is low and less than 20. STRANGE.
	{
		size_t iterations = 0;
		collectChunksNeedingLightUpdate();
		while (!lightUpdateContainer.empty())
		{
			updateChunkLights();
			collectChunksNeedingLightUpdate();
			iterations++;
			if (iterations >= 10)
			{
				break;
			}
		}
	}

	updateChunkMeshes();

	// Entities
	for (auto& pair : entities)
	{
		auto& entity = pair.second;
		entity->update(deltaTime);
	}
}

void World::sortChunkMeshes(const glm::vec3& cameraPos)
{
	// No multithreading for now
	const glm::ivec3 cameraBlockPos = glm::floor(cameraPos);
	const bool cameraMoved = cameraBlockPos != lastChunkMeshSortPos;
	lastChunkMeshSortPos = cameraBlockPos;

	for (auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();
		if (chunk->shouldMeshBeSorted(cameraMoved))
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

void World::clearFrambuffer() const
{
	const auto& bg = visualSettings.backgroundColor;
	glClearColor(bg.r, bg.g, bg.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void World::renderChunks(const Camera& camera) const
{
	faceShader->use();
	{
		// Matrices
		faceShader->setMat4("view", camera.getViewMatrix());
		faceShader->setMat4("projection", camera.getProjectionMatrix());
	
		const auto& fogColor = visualSettings.backgroundColor;
		faceShader->setVec3("fogColor", fogColor.r, fogColor.g, fogColor.b);
		faceShader->setFloat("fogDensity", visualSettings.fogDensity);
		faceShader->setFloat("fogGradient", visualSettings.fogGradient);
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
			for (const auto& command : chunkDrawCommands)
			{
				debugData.renderedFaceCount += command.instanceCount;
			}

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
			for (const auto& command : chunkDrawCommands)
			{
				debugData.renderedFaceCount += command.instanceCount;
			}

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

RaycastResult World::raycast(const glm::dvec3& origin, const glm::dvec3& direction, float maxDistance) const
{
	PROFILE_SCOPE("Raycast", ProfileCategory::General);

	RaycastResult result;

	const glm::dvec3 dir = glm::normalize(direction);

	// DDA algorithm for voxel traversal
	glm::dvec3 currentPos = origin;
	glm::ivec3 blockPos = glm::ivec3(glm::floor(currentPos));

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
			double delta = fabsf(invDir);
			tDelta[i] = delta;
			if (step[i] > 0)
			{
				tMax[i] = (1.0 - glm::fract(currentPos[i])) * delta;
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

			Block block = chunk->getBlockAt(localBlockPos.x, localBlockPos.y, localBlockPos.z);
			const BlockData* blockData = BlockDataBase::getBlockData(block);

			if (blockData->properties.raycastable)
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
	for (const auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();
		chunk->markWholeMeshDirty();
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

void World::unloadChunksOutsideRange()
{
	std::vector<glm::ivec3> chunksToUnload;
	{
		PROFILE_SCOPE("Unload chunks: collect", ProfileCategory::ChunkLoadUnload);

		const int x = lastChunkLoaderPos.x;
		const int y = lastChunkLoaderPos.y;
		const int z = lastChunkLoaderPos.z;

		int renderDistanceSquared = chunkLoadingDistance * chunkLoadingDistance;

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
				});
		}
	}
}

void World::updateChunkLights()
{
	// Using parallelForEach because it will assure that all tasks are done before returning
	
	// Split chunks into two groups in checkboard pattern and update them in two separate passes to avoid racing conditions
	std::vector<Chunk*> groupA;
	groupA.reserve(lightUpdateContainer.size() / 2);

	std::vector<Chunk*> groupB;
	groupB.reserve(lightUpdateContainer.size() / 2);

	for (Chunk* chunk : lightUpdateContainer)
	{
		glm::ivec3 pos = chunk->getPosition();
		if ((pos.x + pos.y + pos.z) & 1)
		{
			groupA.push_back(chunk);
		}
		else
		{
			groupB.push_back(chunk);
		}
	}

	lightUpdateContainer.clear();

	ParallelUtils::parallelForEach(groupA, 1, [](Chunk* chunk)
		{
			chunk->updateLight();
		});
	ParallelUtils::parallelForEach(groupB, 1, [](Chunk* chunk)
		{
			chunk->updateLight();
		});
}

void World::collectChunksNeedingLightUpdate()
{
	PROFILE_SCOPE("Collect chunks needing light update", ProfileCategory::ChunkLight);

	for (const auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();
		if (chunk->hasLightUpdates())
		{
			lightUpdateContainer.push_back(chunk);
		}
	}
}

void World::updateChunkMeshes()
{
	ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
	for (const auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();
		if (chunk->isMeshDirty())
		{
			pool.enqueue([chunk]()
				{
					chunk->updateMesh();
				});
		}
	}
}

void World::collectChunksToRender(std::vector<ChunkRenderInfo>& chunksToRender, const Camera& camera) const
{
	chunksToRender.reserve(chunks.size());

	const Frustum& frustum = camera.getFrustum();
	Box chunkShape(glm::dvec3(0.0), glm::dvec3(CHUNK_SIZE >> 1));

	const glm::dvec3 cameraPosition = camera.getPosition();
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
		glm::dvec3 chunkWorldPosition = glm::dvec3(chunkPosition * CHUNK_SIZE);

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

bool World::placeBlock(const RaycastResult& raycast, Block block)
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
	return true;
}

bool World::breakBlock(const RaycastResult& raycast)
{
	if (!raycast.hit)
	{
		return false;
	}

	updateBlockAt(raycast.hitBlockPosition, Block::Air);
	return true;
}

void World::updateBlockAt(const glm::ivec3& worldPos, Block block)
{
	// Convert world position to chunk position and local position
	glm::ivec3 chunkPos = worldPos >> CHUNK_SIZE_LOG2;
	glm::ivec3 localPos = worldPos & CHUNK_LOWER_BITS_MASK;

	// Get the chunk
	Chunk* chunk = getChunkAt(chunkPos);
	if (!chunk)
	{
		return;
	}

	// Update the block
	chunk->setBlockAt(localPos.x, localPos.y, localPos.z, block);
}

const WorldVisualSettings& World::getWorldVisualSettings() const
{
	return visualSettings;
}

void World::setChunkLoadingDistance(int renderDistance)
{
	chunkLoadingDistance = renderDistance;

	float fogDistance = (chunkLoadingDistance - 0.5f) * CHUNK_SIZE;
	visualSettings.fogDensity = visualSettings.calculateFogDensity(fogDistance, visualSettings.fogGradient);
}

std::optional<Block> World::getBlockAt(const glm::ivec3& globalPosition) const
{
	glm::ivec3 chunkPos = globalPosition >> CHUNK_SIZE_LOG2;

	const Chunk* chunk = getChunkAt(chunkPos);
	if (!chunk)
	{
		return std::nullopt;
	}

	glm::ivec3 localBlockPos = globalPosition & CHUNK_LOWER_BITS_MASK;
	return chunk->getBlockAt(localBlockPos.x, localBlockPos.y, localBlockPos.z);
}

//============================================================================
// ChunkRenderInfo

World::ChunkRenderInfo::ChunkRenderInfo(const Chunk* chunk, unsigned int manhattanDistance) :
	chunk(chunk), manhattanDistance(manhattanDistance)
{
}

//============================================================================