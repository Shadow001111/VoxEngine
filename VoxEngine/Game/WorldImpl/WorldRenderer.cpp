#include "WorldRenderer.h"

#include "../World/Chunk.h"
#include "../World/Chunk/ChunkMeshManager.h"

#include "Core/Profiler.h"

#include "Graphics/TextureLoader.h"

#include "SeamlessPerlinNoise/Perlin.h"

struct ViewRays
{
	glm::vec4 bottomLeft;
	glm::vec4 bottomRight;
	glm::vec4 topLeft;
	glm::vec4 topRight;
};

ViewRays computeViewRays(const Camera& cam)
{
	// Half extents of the view plane at unit distance
	float halfHeight = tan(cam.getFOV() * 0.5f);
	float halfWidth = halfHeight * cam.getAspectRatio();

	// Scale basis vectors
	glm::vec3 forward = cam.getForward();
	glm::vec3 right = glm::vec3(cam.getRight()) * halfWidth;
	glm::vec3 up = glm::vec3(cam.getUp()) * halfHeight;

	ViewRays rays;
	rays.bottomLeft = glm::vec4(forward - right - up, 0);
	rays.bottomRight = glm::vec4(forward + right - up, 0);
	rays.topLeft = glm::vec4(forward - right + up, 0);
	rays.topRight = glm::vec4(forward + right + up, 0);
	return rays;
}


WorldRenderer::ChunkRenderInfo::ChunkRenderInfo(const Chunk* chunk, unsigned int manhattanDistance) :
	chunk(chunk), manhattanDistance(manhattanDistance)
{}


void WorldRenderer::initTextures(const std::vector<std::string>& blockTextureNames)
{
	{
		// Load
		TextureLoader::TextureParams params;
		params.createMipmaps = true;

		PROFILE_SCOPE("Block texture array creation", ProfileCategory::General);
		TextureLoader::createTextureArrayFromImages(blockTextureArray, "res/BlockTextures", blockTextureNames, params);

		blockTextureArray.setParameters(GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT);

		if (Texture::getExtensions().bindless)
		{
			blockTextureArray.initHandle();
			blockTextureArray.makeResident();
		}
	}

	// Perlin noise texture
	{
		TextureLoader::TextureParams params;
		params.desiredChannels = 1;

		const size_t textureSize = 128;

		std::vector<float> data;
		SeamlessPerlinNoise::generatePerlinNoise3D(data, textureSize, textureSize, textureSize, 1.0f / 20.0f, 1, 2.0f, true, 0);

		PROFILE_SCOPE("Noise texture creation", ProfileCategory::General);
		TextureLoader::createTexture3DFromFloatData(tilingPerlinNoise3DTexture, data, textureSize, textureSize, textureSize, params);

		tilingPerlinNoise3DTexture.setParameters(GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);

		if (Texture::getExtensions().bindless)
		{
			tilingPerlinNoise3DTexture.initHandle();
			tilingPerlinNoise3DTexture.makeResident();
		}
	}
}

void WorldRenderer::initBuffers()
{
	chunkDrawCommandBuffer.create(GL_DRAW_INDIRECT_BUFFER, GL_DYNAMIC_DRAW);

	chunkPositionSSBO.create(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW);
	chunkPositionSSBO.bindBase(0);

	chunkNormalSSBO.create(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW);
	chunkNormalSSBO.bindBase(1);

	skyViewRaysUBO.create(GL_UNIFORM_BUFFER);
	skyViewRaysUBO.allocateStorage(sizeof(ViewRays), GL_DYNAMIC_STORAGE_BIT);
	skyViewRaysUBO.bindBase(0);
}

void WorldRenderer::initShaders()
{
	// Bind only the textures which will change their size (if texture will get resized, its handle will gone)
	// TODO: Always pass textures with uniforms, not bind slots. Just check every frame, if id have changed, update uniform.
	// Or update each uniform each frame. Or use binding...

	std::vector<Shader::ShaderSource> sources;

	{
		sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/Chunk/alignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/Chunk/alignedOpaqueFace.frag"}
		};
		alignedOpaqueFaceShader.create(sources);
	}
	{
		sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/Chunk/alignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/Chunk/alignedTranslucentFace.frag"}
		};
		alignedTranslucentFaceShader.create(sources);
	}
	{
		sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/Chunk/nonAlignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/Chunk/nonAlignedOpaqueFace.frag"}
		};
		nonAlignedOpaqueFaceShader.create(sources);
	}
	{
		sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/Chunk/nonAlignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/Chunk/nonAlignedTranslucentFace.frag"}
		};
		nonAlignedTranslucentFaceShader.create(sources);
	}
	{
		sources =
		{
			{GL_COMPUTE_SHADER, "res/Shaders/composite.comp"}
		};
		compositeShader.create(sources);
	}
	{
		sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/voxelMarker.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/voxelMarker.frag"}
		};
		voxelMarkerShader.create(sources);
	}
	{
		sources =
		{
			{GL_COMPUTE_SHADER, "res/Shaders/aurora.comp"}
		};
		auroraShader.create(sources);

		if (Texture::getExtensions().bindless)
		{
			auroraShader.setHandleui64ARB("noiseTex", tilingPerlinNoise3DTexture.getHandle());
		}
	}

	// Set needed uniforms
	{
		const Shader* blockFaceShaders[] =
		{
			&alignedOpaqueFaceShader,
			&alignedTranslucentFaceShader,
			&nonAlignedOpaqueFaceShader,
			&nonAlignedTranslucentFaceShader
		};

		if (Texture::getExtensions().bindless)
		{
			auto blockTextureArrayHandle = blockTextureArray.getHandle();
			for (const Shader* shader : blockFaceShaders)
			{
				shader->setHandleui64ARB("blockTextures", blockTextureArrayHandle);
			}
		}
	}
}

void WorldRenderer::collectAndSortChunksForRendering(const Camera& camera) const
{
	{
		PROFILE_SCOPE("Render: collect chunks", ProfileCategory::Render);

		const LiteFrustum& frustum = camera.getFrustum();
		Box chunkShape(glm::dvec3(0.0), glm::dvec3(CHUNK_SIZE >> 1));

		const glm::dvec3 cameraPosition = camera.getPosition();
		const glm::ivec3 cameraChunkPosition = glm::ivec3(glm::floor(cameraPosition / (double)CHUNK_SIZE));

		chunksToRender.clear();
		chunksToRender.reserve(references.chunks.size());

		for (const auto& [chunkPosition, chunk] : references.chunks)
		{
			if (!chunk->canBeRendered())
			{
				continue;
			}

			// Check is chunk is on frustum
			glm::dvec3 chunkWorldPosition = glm::dvec3(chunkPosition) * (double)CHUNK_SIZE;

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

	{
		PROFILE_SCOPE("Render: sort chunks", ProfileCategory::Render);

		// TODO: Maybe use radix sort? It doesn't take very long too sort though.
		std::sort(chunksToRender.begin(), chunksToRender.end(),
			[](const ChunkRenderInfo& a, const ChunkRenderInfo& b)
			{
				return a.manhattanDistance < b.manhattanDistance;
			});
	}
}

void WorldRenderer::renderOpaqueChunks()
{
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	// Aligned
	{
		PROFILE_SCOPE("Render: collect draw commands", ProfileCategory::Render);

		chunkDrawCommands.clear();
		chunkPositions.clear();
		chunkNormalPacker.clear();
		for (const auto& info : chunksToRender)
		{
			info.chunk->collectAlignedOpaqueRenderData(chunkDrawCommands, chunkPositions, chunkNormalPacker);
		}
	}

	{
		const size_t drawCount = chunkDrawCommands.size();
		if (drawCount > 0)
		{
			for (const auto& command : chunkDrawCommands)
			{
				renderStats.renderedChunkFaceCount += command.instanceCount;
			}

			chunkDrawCommandBuffer.allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer.write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO.allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO.write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));

			const size_t normalCount = chunkNormalPacker.getSize();
			chunkNormalSSBO.allocateMemory(normalCount * sizeof(uint32_t));
			chunkNormalSSBO.write(chunkNormalPacker.getData(), normalCount * sizeof(uint32_t));

			alignedOpaqueFaceShader.use();
			ChunkMeshManager::getInstance().bindAlignedVAO();
			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, (GLsizei)drawCount, 0);
		}
	}

	// Non-aligned
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
				renderStats.renderedChunkFaceCount += command.instanceCount;
			}

			chunkDrawCommandBuffer.allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer.write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO.allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO.write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));

			nonAlignedOpaqueFaceShader.use();
			ChunkMeshManager::getInstance().bindNonAlignedVAO();
			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, (GLsizei)drawCount, 0);
		}
	}
}

void WorldRenderer::renderTranslucentChunks()
{
	//glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	//glBlendFunci(0, GL_ONE, GL_ONE);	// accumulation buffer
	//glBlendFunci(1, GL_ONE, GL_ONE);	// revealage buffer

	// Aligned
	{
		PROFILE_SCOPE("Render: collect draw commands", ProfileCategory::Render);

		chunkDrawCommands.clear();
		chunkPositions.clear();
		chunkNormalPacker.clear();
		for (const auto& info : chunksToRender)
		{
			info.chunk->collectAlignedTranslucentRenderData(chunkDrawCommands, chunkPositions, chunkNormalPacker);
		}
	}

	{
		const size_t drawCount = chunkDrawCommands.size();
		if (drawCount > 0)
		{
			for (const auto& command : chunkDrawCommands)
			{
				renderStats.renderedChunkFaceCount += command.instanceCount;
			}

			chunkDrawCommandBuffer.allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer.write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO.allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO.write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));

			const size_t normalCount = chunkNormalPacker.getSize();
			chunkNormalSSBO.allocateMemory(normalCount * sizeof(uint32_t));
			chunkNormalSSBO.write(chunkNormalPacker.getData(), normalCount * sizeof(uint32_t));

			alignedTranslucentFaceShader.use();
			ChunkMeshManager::getInstance().bindAlignedVAO();
			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, (GLsizei)drawCount, 0);
		}
	}

	// Non-aligned
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
				renderStats.renderedChunkFaceCount += command.instanceCount;
			}

			chunkDrawCommandBuffer.allocateMemory(drawCount * sizeof(DrawArraysIndirectCommand));
			chunkDrawCommandBuffer.write(chunkDrawCommands.data(), drawCount * sizeof(DrawArraysIndirectCommand));

			chunkPositionSSBO.allocateMemory(drawCount * sizeof(glm::ivec3));
			chunkPositionSSBO.write(chunkPositions.data(), drawCount * sizeof(glm::ivec3));

			nonAlignedTranslucentFaceShader.use();
			ChunkMeshManager::getInstance().bindNonAlignedVAO();
			glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, (GLsizei)drawCount, 0);
		}
	}

	glDepthMask(GL_TRUE);
}

void WorldRenderer::renderChunks(const Camera& camera, const FrameBuffer& FBO)
{
	// Set uniforms
	{
		glm::vec3 fogColor = glm::mix(visualSettings.nightBackgroundColor, visualSettings.dayBackgroundColor, references.dayNightCycleValue);

		const glm::ivec3 cameraChunkPos = glm::ivec3(glm::floor(camera.getPosition())) >> CHUNK_SIZE_LOG2;

		auto viewMatrix = camera.getViewMatrixModified(glm::dvec3(CHUNK_SIZE));
		auto projectionMatrix = camera.getProjectionMatrix();

		const Shader* shaders[] =
		{
			&alignedOpaqueFaceShader,
			&alignedTranslucentFaceShader,
			&nonAlignedOpaqueFaceShader,
			&nonAlignedTranslucentFaceShader
		};

		for (int i = 0; i < 4; i++)
		{
			const Shader* shader = shaders[i];
			const bool isTranslucent = i & 1;

			shader->setIvec3("cameraChunkPosition", cameraChunkPos.x, cameraChunkPos.y, cameraChunkPos.z);

			shader->setMat4("view", viewMatrix);
			shader->setMat4("projection", projectionMatrix);

			shader->setVec3("fogColor", fogColor.r, fogColor.g, fogColor.b);
			shader->setFloat("fogDensity", visualSettings.fogDensity);
			shader->setFloat("fogGradient", visualSettings.fogGradient);

			if (isTranslucent)
			{
				shader->setFloat("farPlane", float(chunkRenderDistance * CHUNK_SIZE));
			}

			shader->setFloat("skyLightSub", references.skyLightSub);
		}
	}

	// Collect chunks to render
	collectAndSortChunksForRendering(camera);

	renderStats.renderedChunks = chunksToRender.size();
	renderStats.renderedChunkFaceCount = 0;

	// Bind indirect buffer to allow indirect rendering
	chunkDrawCommandBuffer.bind();

	// Bind textures
	if (!Texture::getExtensions().bindless)
	{
		blockTextureArray.bindUnit(0);
	}

	// Set shared opengl states
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Render chunks
	renderOpaqueChunks();
	renderTranslucentChunks();
}

void WorldRenderer::renderAurora(const Camera& camera, const FrameBuffer& FBO) const
{
	// Get textures
	auto getTextureResult = FBO.getTexture("aurora");
	if (!getTextureResult.has_value())
	{
		std::cerr << "[World][renderAurora]: FBO does not have 'aurora' texture\n";
		return;
	}
	const Texture& auroraTex = *getTextureResult.value();

	getTextureResult = FBO.getTexture("geometryAlpha");
	if (!getTextureResult.has_value())
	{
		std::cerr << "[World][compositePass]: FBO does not have 'geometryAlpha' texture\n";
		return;
	}
	const Texture& geometryAlphaTex = *getTextureResult.value();

	getTextureResult = FBO.getTexture("revealage");
	if (!getTextureResult.has_value())
	{
		std::cerr << "[World][compositePass]: FBO does not have 'revealage' texture\n";
		return;
	}
	const Texture& revealageTex = *getTextureResult.value();

	// Compute rays and pass them to UBO
	auto viewRays = computeViewRays(camera);
	skyViewRaysUBO.write(&viewRays, sizeof(viewRays));

	// Set uniforms
	auroraShader.use();
	auroraShader.setFloat("time", references.appTime);
	auroraShader.setFloat("auroraAlpha", auroraAlpha);

	// Get texture dimensions
	int width = auroraTex.getWidth();
	int height = auroraTex.getHeight();

	// Dispatch compute shader
	const int localSize = 16;
	int groupsX = (width + localSize - 1) / localSize;
	int groupsY = (height + localSize - 1) / localSize;

	glBindImageTexture(0, auroraTex.getID(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
	geometryAlphaTex.bindUnit(1);
	revealageTex.bindUnit(2);
	tilingPerlinNoise3DTexture.bindUnit(3);

	glDispatchCompute(groupsX, groupsY, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void WorldRenderer::compositePass(const FrameBuffer& FBO) const
{
	PROFILE_SCOPE("Render: composite pass", ProfileCategory::Render);

	auto getTextureResult = FBO.getTexture("color");
	if (!getTextureResult.has_value())
	{
		std::cerr << "[World][compositePass]: FBO does not have 'color' texture\n";
		return;
	}
	const Texture& colorTex = *getTextureResult.value();

	getTextureResult = FBO.getTexture("geometryAlpha");
	if (!getTextureResult.has_value())
	{
		std::cerr << "[World][compositePass]: FBO does not have 'geometryAlpha' texture\n";
		return;
	}
	const Texture& geometryAlphaTex = *getTextureResult.value();

	getTextureResult = FBO.getTexture("accumulation");
	if (!getTextureResult.has_value())
	{
		std::cerr << "[World][compositePass]: FBO does not have 'accumulation' texture\n";
		return;
	}
	const Texture& accumulationTex = *getTextureResult.value();

	getTextureResult = FBO.getTexture("revealage");
	if (!getTextureResult.has_value())
	{
		std::cerr << "[World][compositePass]: FBO does not have 'revealage' texture\n";
		return;
	}
	const Texture& revealageTex = *getTextureResult.value();

	getTextureResult = FBO.getTexture("aurora");
	if (!getTextureResult.has_value())
	{
		std::cerr << "[World][compositePass]: FBO does not have 'aurora' texture\n";
		return;
	}
	const Texture& auroraTex = *getTextureResult.value();

	// Get texture dimensions
	int width = colorTex.getWidth();
	int height = colorTex.getHeight();

	// Bind textures
	glBindImageTexture(0, colorTex.getID(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
	geometryAlphaTex.bindUnit(1);
	accumulationTex.bindUnit(2);
	revealageTex.bindUnit(3);
	auroraTex.bindUnit(4);

	// Set uniforms
	compositeShader.use();
	glm::vec3 fogColor = glm::mix(visualSettings.nightBackgroundColor, visualSettings.dayBackgroundColor, references.dayNightCycleValue);
	compositeShader.setVec3("backgroundColor", fogColor.r, fogColor.g, fogColor.z);
	compositeShader.setBool("enableAurora", auroraAlpha > AURORA_THRESHOLD);

	// Dispatch compute shader
	const int localSize = 16;
	int groupsX = (width + localSize - 1) / localSize;
	int groupsY = (height + localSize - 1) / localSize;

	glDispatchCompute(groupsX, groupsY, 1);

	// Ensure computation is complete before subsequent operations
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void WorldRenderer::renderVoxelMarker(const Camera& camera, const RaycastResult& raycast) const
{
	if (!raycast.hit)
	{
		return;
	}

	voxelMarkerShader.use();
	{
		const glm::ivec3 cameraChunkPos = glm::ivec3(glm::floor(camera.getPosition())) >> CHUNK_SIZE_LOG2;
		auto viewMatrix = camera.getViewMatrixModified(glm::dvec3(CHUNK_SIZE));
		auto projectionMatrix = camera.getProjectionMatrix();

		// Camera chunk position
		voxelMarkerShader.setIvec3("cameraChunkPosition", cameraChunkPos.x, cameraChunkPos.y, cameraChunkPos.z);
		voxelMarkerShader.setMat4("view", viewMatrix);
		voxelMarkerShader.setMat4("projection", projectionMatrix);
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
		glm::vec3 pos = raycast.hitPosition;
		voxelMarkerShader.setVec3("position", pos.x, pos.y, pos.z);
		voxelMarkerShader.setFloat("scale", 0.2f);
		voxelMarkerShader.setVec3("color", 1.0f, 0.0f, 0.0f);
		voxelMarkerMesh.draw();
	}
}

WorldRenderer::WorldRenderer(const ReferencesFromWorld& references) :
	references(references)
{
	// Visual settings
	visualSettings.dayBackgroundColor = { 0.52f, 0.8f, 0.92f };
	visualSettings.nightBackgroundColor = { 0.0f, 0.0f, 0.1f };
	visualSettings.fogGradient = 5.0f;
}

void WorldRenderer::init(const std::vector<std::string>& blockTextureNames)
{
	initTextures(blockTextureNames);
	initBuffers();
	initShaders();
}

void WorldRenderer::update()
{
	auroraAlpha = (float)pow(1.0 - references.dayNightCycleValue, 10.0);
}

void WorldRenderer::render(const Camera& camera, const FrameBuffer& FBO, const RaycastResult& raycast)
{
	// Clear buffers
	{
		const float geometryAlpha[] = { 0.0 };
		float accumulation[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float revealage[] = { 0.0f };
		float aurora[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		FBO.clearDrawBuffer("geometryAlpha", geometryAlpha);
		FBO.clearDrawBuffer("accumulation", accumulation);
		FBO.clearDrawBuffer("revealage", revealage);
		if (auroraAlpha > AURORA_THRESHOLD)
		{
			FBO.clearAttachment("aurora", aurora);
		}

		const float depth[] = { 1.0f };
		FBO.clearAttachment("depth", depth);
	}

	// Render chunks
	renderChunks(camera, FBO);

	// Render aurora
	if (auroraAlpha > AURORA_THRESHOLD)
	{
		renderAurora(camera, FBO);
	}

	// Combine everything
	compositePass(FBO);

	// Render voxel marker atop
	renderVoxelMarker(camera, raycast);

	// Set some of render stats
	renderStats.chunkDrawCommandBufferSizeInBytes = chunkDrawCommandBuffer.getCapacity();
	renderStats.chunkPositionBufferSizeInBytes = chunkPositionSSBO.getCapacity();
	renderStats.chunkNormalBufferSizeInBytes = chunkNormalSSBO.getCapacity();
}

void WorldRenderer::setRenderDistance(int renderDistanceInChunks)
{
	this->chunkRenderDistance = renderDistanceInChunks;

	float fogDistance = (renderDistanceInChunks - 0.5f) * CHUNK_SIZE;

	visualSettings.fogDensity = visualSettings.calculateFogDensity(fogDistance, visualSettings.fogGradient);
}
