#include "World.h"

#include "World/Chunk/TerrainGenerator.h"
#include "World/Chunk/ChunkMeshManager.h"

#include "World/Chunk/BlockRegistry.h"

#include "Core/Profiler.h"
#include "Core/Multithreading/ThreadPool.h"

#include "SoundManager.h"

#include <stdexcept>

// BINDINGS
constexpr unsigned BLOCK_TEXTURE_ARRAY_BINDING = 0;

constexpr unsigned OUTPUT_IMAGE_BINDING = 0;
constexpr unsigned OPAQUE_TEX_BINDING = 1;
constexpr unsigned ACCUMULATION_TEX_BINDING = 2;
constexpr unsigned REVEALAGE_TEX_BINDING = 3;


World::World() :
	chunkDrawCommandBuffer(GL_DRAW_INDIRECT_BUFFER, GL_DYNAMIC_DRAW),
	chunkPositionSSBO(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW)
{
	// Visual settings
	visualSettings.backgroundColor = { 0.52f, 0.8f, 0.92f }; // Sky color
	visualSettings.fogGradient = 5.0f;

	// Shaders
	{
		std::vector<Shader::ShaderSource> sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/alignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/alignedOpaqueFace.frag"}
		};
		alignedOpaqueFaceShader = Shader(sources);
	}
	{
		std::vector<Shader::ShaderSource> sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/alignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/alignedTranslucentFace.frag"}
		};
		alignedTranslucentFaceShader = Shader(sources);
	}
	{
		std::vector<Shader::ShaderSource> sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/nonAlignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/nonAlignedOpaqueFace.frag"}
		};
		nonAlignedOpaqueFaceShader = Shader(sources);
	}
	{
		std::vector<Shader::ShaderSource> sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/nonAlignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/nonAlignedTranslucentFace.frag"}
		};
		nonAlignedTranslucentFaceShader = Shader(sources);
	}
	{
		std::vector<Shader::ShaderSource> sources =
		{
			{GL_COMPUTE_SHADER, "res/Shaders/faceComposite.comp"}
		};
		compositeFaceShader = Shader(sources);
		compositeFaceShader.use();
		compositeFaceShader.setInt("outputImage", OUTPUT_IMAGE_BINDING);
		compositeFaceShader.setInt("accumulationTex", ACCUMULATION_TEX_BINDING);
		compositeFaceShader.setInt("revealageTex", REVEALAGE_TEX_BINDING);
		compositeFaceShader.setInt("opaqueTex", OPAQUE_TEX_BINDING);
	}
	{
		std::vector<Shader::ShaderSource> sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/voxelMarker.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/voxelMarker.frag"}
		};
		voxelMarkerShader = Shader(sources);
	}

	// Block data base
	std::vector<std::string> blockTextureNames;
	{
		PROFILE_SCOPE("Blocks registration and assets loading", ProfileCategory::General);
		BlockRegistry::registerBlocks(blockTextureNames);
	}

	// Block textures
	{
		PROFILE_SCOPE("Block texture array creation", ProfileCategory::General);
		blockTextureArray.load("res/Textures", blockTextureNames, 16);
	}

	{
		alignedOpaqueFaceShader.use();
		alignedOpaqueFaceShader.setInt("blockTextures", BLOCK_TEXTURE_ARRAY_BINDING);

		alignedTranslucentFaceShader.use();
		alignedTranslucentFaceShader.setInt("blockTextures", BLOCK_TEXTURE_ARRAY_BINDING);

		nonAlignedOpaqueFaceShader.use();
		nonAlignedOpaqueFaceShader.setInt("blockTextures", BLOCK_TEXTURE_ARRAY_BINDING);

		nonAlignedTranslucentFaceShader.use();
		nonAlignedTranslucentFaceShader.setInt("blockTextures", BLOCK_TEXTURE_ARRAY_BINDING);
	}

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

	// Chunk position SSBO
	chunkPositionSSBO.bindBase(0);

	// Chunks
	Chunk::globalInit();

	// Chunk loaders
	createChunkLoader<SphericalChunkLoader>();

	// Entities
	Entity::world = this;

	// World directory
	try
	{
		const std::string worldName = "Test1";
		std::filesystem::path worldPath = std::filesystem::path("Worlds") / worldName;
		if (!std::filesystem::exists(worldPath))
		{
			if (!std::filesystem::create_directories(worldPath / "Chunks"))
			{
				throw std::runtime_error("Failed to create world directory: " + worldPath.string());
			}
		}

		Chunk::WORLD_PATH = worldPath;
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		throw std::runtime_error("Filesystem error creating world: " + std::string(e.what()));
	}
}

World::~World()
{}

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

	/*size_t maxFacesCount = chunkCount * size_t(CHUNK_VOLUME * 6);
	ChunkMeshManager::getInstance().preallocateMemory(maxFacesCount);

	chunkDrawCommandBuffer->allocateMemory(chunkCount * sizeof(DrawArraysIndirectCommand));
	chunkPositionSSBO->allocateMemory(chunkCount * sizeof(glm::vec3));*/
}

void World::loadChunks(const glm::vec3& playerPos)
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

void World::update(float deltaTime)
{
	// Chunks
	chunkPool.returnProcessingChunksToPool();

	if (!buildBlocksContainer.empty())
	{
		startBuildingChunkBlocks();
	}

	{
		PROFILE_SCOPE("Update chunk blocks", ProfileCategory::ChunkBlocks);

		for (const auto& pair : chunks)
		{
			Chunk* chunk = pair.second.get();
			
			if (chunk->areBlocksBuilt() && chunk->hasStructureBlockUpdates())
			{
				chunk->updateStructureBlocks();
			}
		}
	}

	if (!buildLightContainer.empty())
	{
		startBuildingChunkLights();
	}

	// Sometimes loops forever. Because of sky light propagation.
	// If not limited, it goes forever. But when limited to 20 iterations, next fame iteration count is low and less than 20. STRANGE.
	{
		size_t iterations = 0;
		collectChunksNeedingLightUpdate();
		while (!lightUpdateContainerA.empty() || !lightUpdateContainerB.empty())
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

	updateChunkMeshes();

	// Entities
	for (auto& pair : entities)
	{
		auto& entity = pair.second;
		entity->update(deltaTime);
	}
}

void World::sendChunkMeshesToGPU()
{
	// Send only dirty meshes
	for (auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();
		chunk->askForMeshUpload();
	}
	Chunk::sendMeshesToGPU();
}

void World::collectChunksToRenderAndSortThem(std::vector<ChunkRenderInfo>& chunksToRender, const Camera& camera) const
{
	PROFILE_SCOPE("Render: collect and sort chunks", ProfileCategory::Render);

	chunksToRender.reserve(chunks.size());

	const Frustum& frustum = camera.getFrustum();
	Box chunkShape(glm::dvec3(0.0), glm::dvec3(CHUNK_SIZE >> 1));

	const glm::dvec3 cameraPosition = camera.getPosition();
	const glm::ivec3 cameraChunkPosition = glm::ivec3(glm::floor(cameraPosition / (double)CHUNK_SIZE));

	for (const auto& pair : chunks)
	{
		const Chunk* chunk = pair.second.get();

		if (!chunk->canBeRendered())
		{
			continue;
		}

		// Check is chunk is on frustum
		glm::ivec3 chunkPosition = chunk->getPosition();
		glm::dvec3 chunkWorldPosition = chunkPosition << CHUNK_SIZE_LOG2;

		chunkShape.center = chunkWorldPosition + chunkShape.halfExtents;
		if (!frustum.checkBox(chunkShape))
		{
			continue;
		}

		glm::ivec3 delta = glm::abs(chunkPosition - cameraChunkPosition);

		unsigned int manhattanDistance = delta.x + delta.y + delta.z;
		chunksToRender.emplace_back(chunk, manhattanDistance);
	}

	// Maybe use radix sort? I doesn't take long now anyway.
	std::sort(chunksToRender.begin(), chunksToRender.end(),
		[](const ChunkRenderInfo& a, const ChunkRenderInfo& b)
		{
			return a.manhattanDistance < b.manhattanDistance;
		});
}

void World::renderChunks(const Camera& camera, const OpenGL_FBO* opaqueFBO, const OpenGL_FBO* translucentFBO)
{
	{
		const glm::ivec3 cameraChunkPos = glm::ivec3(glm::floor(camera.getPosition())) >> CHUNK_SIZE_LOG2;
		const auto& fogColor = visualSettings.backgroundColor;

		auto viewMatrix = camera.getViewMatrixModified(glm::dvec3(CHUNK_SIZE));
		auto projectioMatrix = camera.getProjectionMatrix();

		const Shader* shaders[4] =
		{
			&alignedOpaqueFaceShader,
			&alignedTranslucentFaceShader,
			&nonAlignedOpaqueFaceShader,
			&nonAlignedTranslucentFaceShader
		};

		for (const Shader* shader : shaders)
		{
			shader->use();

			shader->setIvec3("cameraChunkPosition", cameraChunkPos.x, cameraChunkPos.y, cameraChunkPos.z);

			shader->setMat4("view", viewMatrix);
			shader->setMat4("projection", projectioMatrix);

			shader->setVec3("fogColor", fogColor.r, fogColor.g, fogColor.b);
			shader->setFloat("fogDensity", visualSettings.fogDensity);
			shader->setFloat("fogGradient", visualSettings.fogGradient);

			shader->setFloat("farPlane", chunkLoadingDistance * CHUNK_SIZE);
		}
	}

	// Collect chunks to render
	std::vector<ChunkRenderInfo> chunksToRender;
	collectChunksToRenderAndSortThem(chunksToRender, camera);

	// Bind resources
	blockTextureArray.bind(BLOCK_TEXTURE_ARRAY_BINDING);

	chunkDrawCommandBuffer.bind();
	chunkPositionSSBO.bind();

	// Set shared opengl states
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Debug data
	debugData.renderedChunks = chunksToRender.size();
	debugData.renderedFaceCount = 0;

	// TODO: Maybe make them global
	std::vector<DrawArraysIndirectCommand> chunkDrawCommands;
	std::vector<glm::ivec3> chunkPositions;

	// Opaque
	opaqueFBO->bind();
	if (!opaqueFBO->isComplete())
	{
		std::cerr << "[Render]: Opaque FBO is not complete!" << std::endl;
		opaqueFBO->unbind();
		return;
	}
	const auto& bg = visualSettings.backgroundColor;
	float bgColor[4] = { bg.r, bg.g, bg.b, 1.0f };
	opaqueFBO->clear();
	opaqueFBO->clearAttachment("color", bgColor);
	renderOpaqueChunks(chunksToRender, chunkDrawCommands, chunkPositions);
	opaqueFBO->unbind();

	// Translucent
	translucentFBO->bind();
	if(!translucentFBO->isComplete())
	{
		std::cerr << "[Render]: Translucent FBO is not complete!" << std::endl;
		translucentFBO->unbind();
		return;
	}

	float clearAccumulation[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float clearRevealage[] = { 1.0f, 0.0f, 0.0f, 0.0f };
	translucentFBO->clearAttachment("accumulation", clearAccumulation);
	translucentFBO->clearAttachment("revealage", clearRevealage);

	renderTranslucentChunks(chunksToRender, chunkDrawCommands, chunkPositions);
	translucentFBO->unbind();

	// Composite
	opaqueFBO->bind();
	compositePass(
		translucentFBO->getTexture("accumulation"),
		translucentFBO->getTexture("revealage"),
		opaqueFBO->getTexture("color")
	);
	opaqueFBO->unbind();
}

void World::renderOpaqueChunks(
	const std::vector<ChunkRenderInfo>& chunksToRender,
	std::vector<DrawArraysIndirectCommand>& chunkDrawCommands,
	std::vector<glm::ivec3>& chunkPositions
	)
{
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	// Aligned
	ChunkMeshManager::getInstance().bindAlignedVAO();
	{
		PROFILE_SCOPE("Render: collect draw commands", ProfileCategory::Render);

		chunkDrawCommands.clear();
		chunkPositions.clear();
		for (const auto& info : chunksToRender)
		{
			info.chunk->collectAlignedOpaqueRenderData(chunkDrawCommands, chunkPositions);
		}
	}

	{
		const size_t drawCount = chunkDrawCommands.size();
		if (drawCount > 0)
		{
			for (const auto& command : chunkDrawCommands)
			{
				debugData.renderedFaceCount += command.instanceCount;
			}

			chunkDrawCommandBuffer.allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer.write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO.allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO.write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));

			alignedOpaqueFaceShader.use();
			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, drawCount, 0);
		}
	}

	// Non-aligned
	ChunkMeshManager::getInstance().bindNonAlignedVAO();
	{
		PROFILE_SCOPE("Render: collect draw commands", ProfileCategory::Render);

		chunkDrawCommands.clear();
		chunkPositions.clear();
		for (const auto& info : chunksToRender)
		{
			info.chunk->collectNonAlignedOpaqueRenderData(chunkDrawCommands, chunkPositions);
		}
	}

	{
		const size_t drawCount = chunkDrawCommands.size();
		if (drawCount > 0)
		{
			for (const auto& command : chunkDrawCommands)
			{
				debugData.renderedFaceCount += command.instanceCount;
			}

			chunkDrawCommandBuffer.allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer.write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO.allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO.write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));

			nonAlignedOpaqueFaceShader.use();
			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, drawCount, 0);
		}
	}
}

void World::renderTranslucentChunks(const std::vector<ChunkRenderInfo>& chunksToRender, std::vector<DrawArraysIndirectCommand>& chunkDrawCommands, std::vector<glm::ivec3>& chunkPositions)
{
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunci(0, GL_ONE, GL_ONE);					// accumulation buffer
	glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);	// revealage buffer
	glBlendEquation(GL_FUNC_ADD);

	// Aligned
	ChunkMeshManager::getInstance().bindAlignedVAO();
	{
		PROFILE_SCOPE("Render: collect draw commands", ProfileCategory::Render);

		chunkDrawCommands.clear();
		chunkPositions.clear();
		for (const auto& info : chunksToRender)
		{
			info.chunk->collectAlignedTranslucentRenderData(chunkDrawCommands, chunkPositions);
		}
	}

	{
		const size_t drawCount = chunkDrawCommands.size();
		if (drawCount > 0)
		{
			for (const auto& command : chunkDrawCommands)
			{
				debugData.renderedFaceCount += command.instanceCount;
			}

			chunkDrawCommandBuffer.allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer.write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO.allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO.write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));

			alignedTranslucentFaceShader.use();
			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, drawCount, 0);
		}
	}

	// Non-aligned
	ChunkMeshManager::getInstance().bindNonAlignedVAO();
	{
		PROFILE_SCOPE("Render: collect draw commands", ProfileCategory::Render);

		chunkDrawCommands.clear();
		chunkPositions.clear();
		for (const auto& info : chunksToRender)
		{
			info.chunk->collectNonAlignedTranslucentRenderData(chunkDrawCommands, chunkPositions);
		}
	}

	{
		const size_t drawCount = chunkDrawCommands.size();
		if (drawCount > 0)
		{
			for (const auto& command : chunkDrawCommands)
			{
				debugData.renderedFaceCount += command.instanceCount;
			}

			chunkDrawCommandBuffer.allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer.write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO.allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO.write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));

			nonAlignedTranslucentFaceShader.use();
			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, drawCount, 0);
		}
	}

	glDepthMask(GL_TRUE);
}

void World::compositePass(const OpenGL_Texture& accumTex, const OpenGL_Texture& revTex, const OpenGL_Texture& colorTex) const
{
	const OpenGL_Texture& outputTex = colorTex;

	// Get texture dimensions
	int width = outputTex.getWidth();
	int height = outputTex.getHeight();

	// Bind textures
	glBindImageTexture(OUTPUT_IMAGE_BINDING, outputTex.getID(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
	glBindTextureUnit(ACCUMULATION_TEX_BINDING, accumTex.getID());
	glBindTextureUnit(REVEALAGE_TEX_BINDING, revTex.getID());
	glBindTextureUnit(OPAQUE_TEX_BINDING, colorTex.getID());

	// Dispatch compute shader
	const int localSize = 16;
	int groupsX = (width + localSize - 1) / localSize;
	int groupsY = (height + localSize - 1) / localSize;

	compositeFaceShader.use();
	glDispatchCompute(groupsX, groupsY, 1);

	// Ensure computation is complete before subsequent operations
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void World::renderVoxelMarker(const Camera& camera, const RaycastResult& raycast) const
{
	if (!raycast.hit)
	{
		return;
	}

	voxelMarkerShader.use();
	{
		// Camera chunk position
		const glm::ivec3 cameraChunkPos = glm::ivec3(glm::floor(camera.getPosition())) >> CHUNK_SIZE_LOG2;
		voxelMarkerShader.setIvec3("cameraChunkPosition", cameraChunkPos.x, cameraChunkPos.y, cameraChunkPos.z);

		// Matrices
		voxelMarkerShader.setMat4("view", camera.getViewMatrixModified(glm::dvec3(CHUNK_SIZE)));
		voxelMarkerShader.setMat4("projection", camera.getProjectionMatrix());
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

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);

	{
		const auto& pos = placePos;
		voxelMarkerShader.setVec3("position", pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f);
		voxelMarkerShader.setFloat("scale", 1.01f);
		voxelMarkerShader.setVec3("color", 1.0f, 0.0f, 1.0f);
		voxelMarkerMesh.draw();
	}
	{
		const auto& pos = raycast.hitPosition;
		voxelMarkerShader.setVec3("position", pos.x, pos.y, pos.z);
		voxelMarkerShader.setFloat("scale", 0.2f);
		voxelMarkerShader.setVec3("color", 1.0f, 0.0f, 0.0f);
		voxelMarkerMesh.draw();
	}
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
			chunk = getChunkAt(cachedChunkPos);
		}

		// Check current block
		if (chunk && chunk->getState() > Chunk::State::BuildingBlocks)
		{
			// Local block position within chunk
			glm::ivec3 localBlockPos = blockPos & CHUNK_LOWER_BITS_MASK;

			BlockID block = chunk->getBlockAt(localBlockPos.x, localBlockPos.y, localBlockPos.z);
			const BlockData* blockData = BlockRegistry::getBlockDataByID(block);
			if (blockData && blockData->properties.raycastable)
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
		chunk->markMeshDirty();
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
	debugData.totalFaceCapacityInBytes = (
		ChunkMeshManager::getInstance().getAlignedInstanceVBO().getCapacity() +
		ChunkMeshManager::getInstance().getNonAlignedInstanceVBO().getCapacity()
		);

	debugData.loadedChunksCount = chunks.size();

	debugData.chunkDrawCommandBufferSizeInBytes = chunkDrawCommandBuffer.getCapacity();
	debugData.chunkPositionBufferSizeInBytes = chunkPositionSSBO.getCapacity();

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

void World::loadChunk(const glm::ivec3& position)
{
	// Check if chunk already exists
	if (chunkExistsAt(position))
	{
		std::cerr << "[LoadChunk]: Chunk is already loaded.\n";
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
	std::unique_ptr<Chunk> chunk = chunkPool.acquire();
	chunk->addLoader();
	chunk->init(position, neighbors);

	{
		std::lock_guard<std::mutex> lock(buildBlocksMutex);
		buildBlocksContainer.insert(chunk.get());
	}

	chunks.emplace(position, std::move(chunk));
}

void World::unloadChunk(const glm::ivec3& position)
{
	const auto& it = chunks.find(position);
	if (it == chunks.end())
	{
		std::cerr << "[UnloadChunk]: Chunk isn't in map.\n";
		return;
	}
	it->second->removeLoader();
	chunkPool.release(std::move(it->second));
	chunks.erase(it);
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
			if (chunk->getState() != Chunk::State::NotInitialized_NeedsBlocks)
			{
				continue;
			}
			ASSERT(!chunk->areBlocksBuilt());
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
				});
		}
	}
}

void World::collectChunksNeedingLightUpdate()
{
	PROFILE_SCOPE("Collect chunks needing light update", ProfileCategory::ChunkLight);

	for (const auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();
		if (chunk->hasLightUpdates())
		{
			glm::ivec3 pos = chunk->getPosition();
			if ((pos.x ^ pos.y ^ pos.z) & 1)
			{
				lightUpdateContainerA.push_back(chunk);
			}
			else
			{
				lightUpdateContainerB.push_back(chunk);
			}
		}
	}
}

void World::updateChunkLights()
{
	// Using parallelForEach because it will assure that all tasks are done before returning
	// Update chunks in two separate passes in checkboard pattern to avoid racing conditions
	ParallelUtils::parallelForEach(lightUpdateContainerA, 1, [](Chunk* chunk)
		{
			chunk->updateLight();
		});
	ParallelUtils::parallelForEach(lightUpdateContainerB, 1, [](Chunk* chunk)
		{
			chunk->updateLight();
		});

	lightUpdateContainerA.clear();
	lightUpdateContainerB.clear();
}

void World::updateChunkMeshes()
{
	ThreadPool& pool = ParallelUtils::getGlobalThreadPool();
	for (const auto& pair : chunks)
	{
		Chunk* chunk = pair.second.get();
		if (chunk->shouldMeshBeUpdated())
		{
			pool.enqueue([chunk]()
				{
					chunk->updateMesh();
				});
		}
	}
}

bool World::placeBlock(const RaycastResult& raycast, BlockID block)
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

	const BlockData* blockData = BlockRegistry::getBlockDataByID(block);
	if (blockData && !blockData->sounds.placeSounds.empty())
	{
		// Choose random sounds from vector
		const auto& sounds = blockData->sounds.placeSounds;
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

	updateBlockAt(raycast.hitBlockPosition, BlockRegistry::getBlockID("core:air"));

	const BlockData* blockData = BlockRegistry::getBlockDataByID(raycast.hitBlock);
	if (blockData && !blockData->sounds.breakSounds.empty())
	{
		// Choose random sounds from vector
		const auto& sounds = blockData->sounds.breakSounds;
		int soundIndex = rand() % sounds.size();
		auto& sndMgr = SoundManager::getInstance();
		sndMgr.play("block/break/" + sounds[soundIndex]);
	}

	return true;
}

void World::updateBlockAt(const glm::ivec3& worldPos, BlockID block)
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
	// TODO: Can possibly break something if chunk is in the middle of processing
	chunk->setBlockAt(localPos.x, localPos.y, localPos.z, block);
}

const WorldVisualSettings& World::getWorldVisualSettings() const
{
	return visualSettings;
}

void World::setChunkLoadingDistance(int renderDistance)
{
	// TODO: Set camera far plane dynamicly
	chunkLoadingDistance = renderDistance;

	float fogDistance = (chunkLoadingDistance - 0.5f) * CHUNK_SIZE;
	visualSettings.fogDensity = visualSettings.calculateFogDensity(fogDistance, visualSettings.fogGradient);
}

std::optional<BlockID> World::getBlockAt(const glm::ivec3& globalPosition) const
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