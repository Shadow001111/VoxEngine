#include "World.h"

#include "Profiler.h"
#include "ThreadPool.h"

#include <iostream>

//============================================================================
// World

World::World()
{
	// Shaders
	std::vector<Shader::ShaderSource> faceShaderSources =
	{
		{GL_VERTEX_SHADER, "res/Shaders/face.vert"},
		{GL_FRAGMENT_SHADER, "res/Shaders/face.frag"}
	};

	faceShader = std::make_unique<Shader>(faceShaderSources);
	faceShaderSources.clear();

	// Block textures
	std::vector<std::string> blockTextureNames;
	{
		PROFILE_SCOPE("BlockTextureIDDatabase build", ProfileCategory::General);
		Chunk::blockTextureDatabase.build(blockTextureNames);
	}
	{
		PROFILE_SCOPE("Block texture array creation", ProfileCategory::General);
		blockTextureArray = std::make_unique<BlockTextureArray>("res/Textures", blockTextureNames, 0, 16);
		blockTextureArray->bind();
		faceShader->setInt("blockTextures", blockTextureArray->getUnit());
	}
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
		PROFILE_SCOPE("Send chunks to blocksBuildChunkContainer", ProfileCategory::ChunkBlocks);

		std::lock_guard<std::mutex> lock(blocksBuildMutex);
		for (Chunk* chunkPtr : chunksToSend)
		{
			blocksBuildChunkContainer.insert(chunkPtr);
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

void World::render(const Camera& camera) const
{
	faceShader->use();
	{
		// Matrices
		faceShader->setMat4("view", camera.getViewMatrix());
		faceShader->setMat4("projection", camera.getProjectionMatrix());

		// Fog
		const auto& fogColor = visuals.backgroundColor;
		faceShader->setVec3("fogColor", fogColor.x, fogColor.y, fogColor.z);
		faceShader->setFloat("fogDensity", visuals.fogDensity);
		faceShader->setFloat("fogGradient", visuals.fogGradient);
	}

	// TODO: Use ssbo for chunk's position. Maybe it's faster? Though takes much more memory.

	std::vector<ChunkRenderInfo> chunksToRender;
	{
		PROFILE_SCOPE("Render: collect chunks", ProfileCategory::Render);

		collectChunksToRender(chunksToRender, camera);

		std::sort(chunksToRender.begin(), chunksToRender.end(),
			[](const ChunkRenderInfo& a, const ChunkRenderInfo& b)
			{
				return a.distanceSquared < b.distanceSquared;
			});
	}
	{
		PROFILE_SCOPE("Render", ProfileCategory::Render);

		renderedFaceCount = 0;
		for (const auto& info : chunksToRender)
		{
			// Set chunk position
			glm::vec3 chunkWorldPosition = info.chunkWorldPosition;
			faceShader->setVec3("chunkPosition", chunkWorldPosition.x, chunkWorldPosition.y, chunkWorldPosition.z);

			info.chunk->render(); // Takes most of the time
			renderedFaceCount += info.chunk->getFaceCount();
		}
	}
}

void World::rebuildAllChunkMeshes()
{
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

void World::getChunkMeshesInfo(size_t& totalFaces, size_t& totalFaceCapacity, size_t& potentialMaximumCapacity, size_t& renderedFaceCount)
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
	renderedFaceCount = this->renderedFaceCount;
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

		int renderDistanceSquared = renderDistance * renderDistance;

		PROFILE_SCOPE("Unload chunks: collect", ProfileCategory::ChunkLoadUnload);
		for (const auto& pair : chunks)
		{
			const Int3& pos = pair.first;
			
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

	chunks.emplace(chunkPosition, std::move(chunk)); // Takes much time when many chunks are being loaded
}

void World::startBuildingChunkBlocks()
{
	// Maybe add PROFILE_SCOPE inside Chunk::buildBlocks. Make Profiler thread safe.
	PROFILE_SCOPE("Start building chunk blocks", ProfileCategory::ChunkBlocks);

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
		PROFILE_SCOPE("Collect chunks for mesh building", ProfileCategory::ChunkMesh);

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
		PROFILE_SCOPE("Send chunks to mesh building", ProfileCategory::ChunkMesh);

		ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
		for (Chunk* chunk : chunksToProcess)
		{
			pool.enqueue([chunk]()
				{
					// Build mesh in background thread (no OpenGL calls here)
					chunk->buildMesh();

					// Mark as chunk as Ready. His mesh can be not on the GPU yet.
					chunk->setState(Chunk::State::Ready);

					// TODO: Issue: Chunk's mesh is flickering.
					// Settings Ready state should be done in Chunk::sendMedhesToGPU
					// Even if yes, mesh is flickering even more. It takes more time to send mesh to GPU, but we set Ready state immediately.
				});
		}
	}
}

void World::collectChunksToRender(std::vector<ChunkRenderInfo>& chunksToRender, const Camera& camera) const
{
	const float CHUNK_SIZE_FLOAT = static_cast<float>(CHUNK_SIZE);

	const Frustum& frustum = camera.getFrustum();
	Box chunkShape(glm::vec3(0.0f), glm::vec3(CHUNK_SIZE_FLOAT * 0.5f));

	glm::vec3 cameraPosition = camera.getPosition();

	chunksToRender.reserve(chunks.size());

	for (const auto& pair : chunks)
	{
		const Chunk* chunk = pair.second.get();

		// Check if mesh is ready
		if (chunk->getState() != Chunk::State::Ready)
		{
			continue;
		}

		// Check if mesh isn't empty
		if (chunk->getFaceCount() == 0)
		{
			continue;
		}

		// Check is chunk is on frustum
		Int3 chunkPosition = chunk->getPosition();
		glm::vec3 chunkPositionGlm = glm::vec3(chunkPosition.x, chunkPosition.y, chunkPosition.z);
		glm::vec3 chunkWorldPosition = chunkPositionGlm * CHUNK_SIZE_FLOAT;

		chunkShape.center = chunkWorldPosition + chunkShape.halfExtents;
		if (!frustum.checkBox(chunkShape))
		{
			continue;
		}

		glm::vec3 chunkCenter = chunkWorldPosition + chunkShape.halfExtents;
		glm::vec3 diff = chunkCenter - cameraPosition;

		float distanceSquared = glm::dot(diff, diff);
		chunksToRender.emplace_back(chunk, chunkWorldPosition, distanceSquared);
	}
}

//============================================================================
// ChunkPool

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

//============================================================================
// Visuals

float World::VisualSettings::calculateFogDensity(float renderDistance_, float fogGradient_)
{
	return powf(-logf(1e-3f), 1.0f / fogGradient_) / renderDistance_;
}

float World::VisualSettings::calculateFogGradient(float renderDistance_, float fogDensity_)
{
	return logf(-logf(1e-3f)) / logf(renderDistance_ * fogDensity_);
}

//============================================================================
// ChunkRenderInfo

World::ChunkRenderInfo::ChunkRenderInfo(const Chunk* chunk, const glm::vec3& chunkWorldPosition, float distanceSquared) :
	chunk(chunk), chunkWorldPosition(chunkWorldPosition), distanceSquared(distanceSquared)
{
}

//============================================================================