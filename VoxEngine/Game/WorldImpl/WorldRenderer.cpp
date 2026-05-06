#include "WorldRenderer.h"

#include "../World/Chunk.h"
#include "../World/ChunkRegion.h"

#include "Graphics/TextureLoader.h"

#include "NoiseLib/Perlin.h"

#include <iostream>

static_assert((-1 >> 1) == -1, "This compiler does not use arithmetic right shift for signed integers");
static_assert((-1 << 1) == -2, "This compiler does not use arithmetic left shift for signed integers");

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


void debugPrintCompressionInfo(const Texture& texture)
{
	GLuint id = texture.getID();

	GLint isCompressed = 0;
	glGetTextureLevelParameteriv(id, 0, GL_TEXTURE_COMPRESSED, &isCompressed);

	if (!isCompressed)
	{
		std::cerr << "[TextureDebug]: Texture " << id << " is NOT compressed.\n";
		return;
	}

	GLint compressedSize = 0;
	glGetTextureLevelParameteriv(id, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compressedSize);

	GLint internalFormat = 0;
	glGetTextureLevelParameteriv(id, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);

	GLint width = 0, height = 0, depth = 0;
	glGetTextureLevelParameteriv(id, 0, GL_TEXTURE_WIDTH, &width);
	glGetTextureLevelParameteriv(id, 0, GL_TEXTURE_HEIGHT, &height);
	glGetTextureLevelParameteriv(id, 0, GL_TEXTURE_DEPTH, &depth);

	int layers = std::max(depth, 1);

	int uncompressedSize = width * height * layers * 4; // assume RGBA8 baseline
	float ratio = static_cast<float>(uncompressedSize) / static_cast<float>(compressedSize);

	std::cout << "[TextureDebug]: Texture " << id << "\n"
		<< "  Internal format : " << texture.getInternalFormatName() << "\n"
		<< "  Compressed size : " << compressedSize << " bytes\n"
		<< "  Uncompressed est: " << uncompressedSize << " bytes\n"
		<< "  Ratio           : " << ratio << ":1\n";
}


WorldRenderer::ChunkRenderInfo::ChunkRenderInfo() :
	chunk(nullptr), manhattanDistance(0)
{}

WorldRenderer::ChunkRenderInfo::ChunkRenderInfo(const Chunk* chunk, unsigned int manhattanDistance) :
	chunk(chunk), manhattanDistance(manhattanDistance)
{}

void WorldRenderer::initTextures(const std::vector<std::string>& blockTextureNames)
{
	{
		// Load
		TextureLoader::TextureLoadParams textureLoadParametrs;
		textureLoadParametrs.createMipmaps = true;
		textureLoadParametrs.compression = TextureCompression::Format::NONE;

		{
			TRACY_SCOPE_NC("Block texture array creation", ProfileCategory::General);
			TextureLoader::createTextureArrayFromImages(blockTextureArray, "res/BlockTextures", blockTextureNames, textureLoadParametrs);
		}

		//debugPrintCompressionInfo(blockTextureArray);

		Texture::Parameters textureParametrs
		{
			.minFilter = GL_NEAREST_MIPMAP_LINEAR,
			.magFilter = GL_NEAREST,
			.anisotropy = 999.0f
		};

		blockTextureArray.setParameters(textureParametrs);

		if (Texture::getExtensions().bindless)
		{
			blockTextureArray.initHandle();
			blockTextureArray.makeResident();
		}
	}

	// Perlin noise texture
	{
		using NoiseGenerator3D = NoiseLib::Base::BaseNoiseGenerator<
			NoiseLib::Perlin::simd2DSeamless,
			NoiseLib::Perlin::simd3DSeamless,
			true,
			false,
			true
		>;

		TextureLoader::TextureLoadParams textureLoadParametrs;
		textureLoadParametrs.desiredChannels = 1;

		const size_t textureSize = 128;

		std::vector<float> data(textureSize * textureSize * textureSize);
		NoiseGenerator3D::gen3D(
			data.data(),
			0,
			{ textureSize, textureSize, textureSize },
			1.0f / 20.0f,
			{0, 0, 0}
		);

		{
			TRACY_SCOPE_NC("Noise texture creation", ProfileCategory::General);
			TextureLoader::createTexture3DFromFloatData(tilingPerlinNoise3DTexture, data, textureSize, textureSize, textureSize, textureLoadParametrs);
		}

		Texture::Parameters textureParametrs
		{
			.minFilter = GL_LINEAR,
			.magFilter = GL_LINEAR
		};

		tilingPerlinNoise3DTexture.setParameters(textureParametrs);

		if (Texture::getExtensions().bindless)
		{
			tilingPerlinNoise3DTexture.initHandle();
			tilingPerlinNoise3DTexture.makeResident();
		}
	}
}

void WorldRenderer::initBuffers()
{
	//chunkDrawCommandBuffer.create(GL_DRAW_INDIRECT_BUFFER);
	//
	//chunkPositionSSBO.create(GL_SHADER_STORAGE_BUFFER);
	//chunkPositionSSBO.bindBase(0);

	skyViewRaysUBO.create(GL_UNIFORM_BUFFER);
	skyViewRaysUBO.allocateStorage(sizeof(ViewRays), GL_DYNAMIC_STORAGE_BIT);
	skyViewRaysUBO.bindBase(0);
}

void WorldRenderer::initShaders()
{
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
			{GL_VERTEX_SHADER, "res/Shaders/Chunk/unalignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/Chunk/unalignedOpaqueFace.frag"}
		};
		unalignedOpaqueFaceShader.create(sources);
	}
	{
		sources =
		{
			{GL_VERTEX_SHADER, "res/Shaders/Chunk/unalignedFace.vert"},
			{GL_FRAGMENT_SHADER, "res/Shaders/Chunk/unalignedTranslucentFace.frag"}
		};
		unalignedTranslucentFaceShader.create(sources);
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
			&unalignedOpaqueFaceShader,
			&unalignedTranslucentFaceShader
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

void WorldRenderer::collectChunksForRendering(const Camera& camera) const
{
	constexpr int CHUNK_REGION_SIZE_IN_BLOCKS = CHUNK_REGION_SIZE * CHUNK_SIZE;

	TRACY_SCOPE_NC("Collect chunks", ProfileCategory::Render);

	// Get regions
	const auto& regions = Chunk::managerInstances->chunkRegion.getRegionMap();

	// Clear and reserve
	chunksToRender.clear();
	chunksToRender.reserve(regions.size() * CHUNK_REGION_VOLUME);

	// Get frustum
	const auto& frustum = camera.getFrustum();

	// Prepare boxes
	Box chunkShape(glm::dvec3(0.0), glm::dvec3(CHUNK_SIZE >> 1));
	Box regionShape(glm::dvec3(0.0), glm::dvec3(CHUNK_REGION_SIZE_IN_BLOCKS >> 1));

	// Get camera positions
	const glm::dvec3 cameraPosition = camera.getPosition();
	const glm::ivec3 cameraChunkPosition = glm::ivec3(glm::floor(cameraPosition / (double)CHUNK_SIZE));

	// Main loop
	for (const auto& [regionPosition, region] : regions)
	{
		// Check if region has chunks to render
		if (!region->hasRenderChunks())
		{
			continue;
		}

		// Get region world position and set shape center
		glm::dvec3 regionWorldPosition = glm::dvec3(regionPosition * CHUNK_REGION_SIZE_IN_BLOCKS);
		regionShape.center = regionWorldPosition + regionShape.halfExtents;

		// Check if region is visible
		if (!frustum.checkBox(regionShape))
		{
			continue;
		}

		// Iterate over chunks in region
		for (const Chunk* chunk : region->getChunks())
		{
			if (chunk == nullptr || !chunk->canBeRendered())
			{
				continue;
			}

			// Get chunk position
			glm::ivec3 chunkPosition = chunk->getPosition();

			// Get chunk world position and set shape center
			glm::dvec3 chunkWorldPosition = glm::dvec3(chunkPosition << CHUNK_SIZE_LOG2);
			chunkShape.center = chunkWorldPosition + chunkShape.halfExtents;

			// Check if chunk is visible
			if (!frustum.checkBox(chunkShape))
			{
				continue;
			}

			// Calculate distance from chunk to camera
			glm::ivec3 delta = glm::abs(chunkPosition - cameraChunkPosition);
			unsigned int manhattanDistance = delta.x + delta.y + delta.z;

			// Add to render array
			chunksToRender.emplace_back(chunk, manhattanDistance);
		}
	}
}

void WorldRenderer::collectChunksForRenderingWithConnectivity(const Camera& camera) const
{
	TRACY_SCOPE_NC("Collect chunks", ProfileCategory::Render);

	chunksToRender.clear();

	const glm::dvec3 cameraPosition = camera.getPosition();
	const glm::ivec3 cameraChunkPos = glm::ivec3(glm::floor(cameraPosition / (double)CHUNK_SIZE));
	const auto& frustum = camera.getFrustum();

	Box chunkShape(glm::dvec3(0.0), glm::dvec3(CHUNK_SIZE >> 1));

	// Find start chunk
	const Chunk* startChunk = nullptr;
	{
		const glm::ivec3 regionPos = ChunkRegion::getRegionPosition(cameraChunkPos);
		const auto& regionMap = Chunk::managerInstances->chunkRegion.getRegionMap();
		const auto  regionIt = regionMap.find(regionPos);
		if (regionIt != regionMap.end())
		{
			const size_t idx = ChunkRegion::getChunkIndexInRegion(cameraChunkPos);
			startChunk = regionIt->second->getChunks()[idx];
		}
	}

	if (startChunk == nullptr)
	{
		collectChunksForRendering(camera);
		return;
	}

	// Region frustum culling
	{
		constexpr int CHUNK_REGION_SIZE_IN_BLOCKS = CHUNK_REGION_SIZE * CHUNK_SIZE;
		Box regionShape(glm::dvec3(0.0), glm::dvec3(CHUNK_REGION_SIZE_IN_BLOCKS >> 1));
		for (const auto& [position, region] : Chunk::managerInstances->chunkRegion.getRegionMap())
		{
			regionShape.center = glm::dvec3(position * CHUNK_REGION_SIZE_IN_BLOCKS) + regionShape.halfExtents;
			region->isFrustumCulled = !frustum.checkBox(regionShape);
		}
	}

	// --- BFS ---

	// CHANGE: Queue node stores a raw pointer into the visited map's value byte.
	//         Entry bits accumulate there live, so any direction arriving before
	//         this node is popped is automatically folded in at pop time.
	//         This means each chunk is enqueued exactly ONCE regardless of how
	//         many faces it is entered from.
	struct QueueNode
	{
		const Chunk* chunk;
		uint8_t* visitedPtr; // live; accumulates entry-direction bits
	};

	static robin_hood::unordered_flat_map<const Chunk*, uint8_t> floodFillVisited;
	floodFillVisited.clear();

	static std::vector<QueueNode> bfsQueue;
	bfsQueue.clear();

	// Start chunk: 0xFF = reachable from every direction
	auto& startVisited = floodFillVisited.emplace(startChunk, uint8_t(0xFF)).first->second;
	bfsQueue.push_back({ startChunk, &startVisited });

	if (startChunk->canBeRendered())
	{
		const glm::ivec3 d = glm::abs(startChunk->getPosition() - cameraChunkPos);
		chunksToRender.emplace_back(startChunk, (unsigned)(d.x + d.y + d.z));
	}

	std::array<int, 6> chunkNeighborIndices;
	for (int i = 0; i < 6; i++)
		chunkNeighborIndices[i] = Chunk::getSideNeighborIndex(i);

	for (size_t head = 0; head < bfsQueue.size(); head++)
	{
		const QueueNode node = bfsQueue[head];
		const Chunk* chunk = node.chunk;

		// Read all entry directions accumulated so far (not just the one
		//         we were pushed with). Other neighbours may have added bits
		//         between when this node was enqueued and now.
		const uint8_t fromDirsMask = *node.visitedPtr;

		// Pre-compute the union of reachable exit faces across ALL entry
		//         directions in one pass, outside the neighbor loop.
		//         Old: connectivity tested once per (entry, exit) inside the loop.
		//         New: one O(36) precompute, then the inner check is a single bit test.
		uint8_t reachableExits;
		if (fromDirsMask == 0xFF)
		{
			reachableExits = 0x3F; // all 6 exits
		}
		else
		{
			reachableExits = 0;
			const auto connectivityMatrix = chunk->getConnectivityMatrix();
			for (int fd = 0; fd < 6; fd++)
			{
				if (!(fromDirsMask & (1u << fd))) continue;
				for (int td = 0; td < 6; td++)
					if (connectivityMatrix.read(fd, td))
						reachableExits |= static_cast<uint8_t>(1u << td);
			}
		}

		const auto& neighborChunks = chunk->getNeighbors();

		for (int toDir = 0; toDir < 6; toDir++)
		{
			if (!(reachableExits & (1u << toDir)))
				continue;

			const Chunk* neighbor = neighborChunks[chunkNeighborIndices[toDir]];
			if (neighbor == nullptr)
				continue;

			if (neighbor->getParentRegion()->isFrustumCulled)
				continue;

			const glm::ivec3 chunkPos = neighbor->getPosition();
			const glm::dvec3 chunkWorldPos = glm::dvec3(chunkPos << CHUNK_SIZE_LOG2);
			chunkShape.center = chunkWorldPos + chunkShape.halfExtents;
			if (!frustum.checkBox(chunkShape))
				continue;

			const int     neighborFromDir = toDir ^ 1;
			const uint8_t entryBit = static_cast<uint8_t>(1u << neighborFromDir);

			auto [visIt, inserted] = floodFillVisited.emplace(neighbor, uint8_t(0));
			if (visIt->second & entryBit)
				continue; // this (chunk, direction) pair is already accounted for

			visIt->second |= entryBit;

			if (inserted)
			{
				if (neighbor->canBeRendered())
				{
					const glm::ivec3 d = glm::abs(chunkPos - cameraChunkPos);
					chunksToRender.emplace_back(neighbor, (unsigned)(d.x + d.y + d.z));
				}
				bfsQueue.push_back({ neighbor, &visIt->second });
			}
		}
	}
}

void WorldRenderer::sortChunksForRendering() const
{
	TRACY_SCOPE_NC("Sort chunks", ProfileCategory::Render);

	// Find max distance
	unsigned int maxDistance = 0;
	for (const auto& info : chunksToRender)
	{
		maxDistance = std::max(maxDistance, info.manhattanDistance);
	}

	// Create array of count
	const size_t bucketCount = maxDistance + 1;
	sortingCountArray.resize(bucketCount);
	std::fill(sortingCountArray.begin(), sortingCountArray.end(), 0);

	// Count occurrences of each distance
	for (const auto& info : chunksToRender)
	{
		sortingCountArray[info.manhattanDistance]++;
	}

	// Calculate prefix sums (positions)
	size_t total = 0;
	for (size_t i = 0; i < bucketCount; i++)
	{
		auto temp = sortingCountArray[i];
		sortingCountArray[i] = total;
		total += temp;
	}

	// Output
	sortingChunkOutputArray.resize(chunksToRender.size());

	for (const auto& info : chunksToRender)
	{
		auto priority = info.manhattanDistance;
		sortingChunkOutputArray[sortingCountArray[priority]++].chunk = info.chunk; // Move pointer only. It doesn't break anything, since we don't use distance further in render.
	}

	chunksToRender.swap(sortingChunkOutputArray);
}

void WorldRenderer::partitionChunksByLayer() const
{
	TRACY_SCOPE_NC("Partition chunks by layer", ProfileCategory::Render);

	// Reset per-layer containers, but keep allocated memory.
	for (auto& v : chunksToRenderPerLayer)
	{
		v.clear();
		v.reserve(chunksToRender.size());
	}

	for (const auto& info : chunksToRender)
	{
		for (int layer = 0; layer < (int)MeshLayer::Count; layer++)
		{
			if (info.chunk->hasFacesToRender(static_cast<MeshLayer>(layer)))
			{
				chunksToRenderPerLayer[layer].emplace_back(info);
			}
		}
	}
}

void WorldRenderer::ensureCapacityForChunkRenderBuffers(size_t drawCount)
{
	// Draw commands
	{
		const size_t bufferCapacity = drawCount * sizeof(DrawArraysIndirectCommand);
		if (chunkDrawCommandBuffer.getCapacity() < bufferCapacity)
		{
			chunkDrawCommandBuffer.create(GL_DRAW_INDIRECT_BUFFER);
			chunkDrawCommandBuffer.allocateStorage(bufferCapacity, GL_DYNAMIC_STORAGE_BIT);

			// Bind indirect buffer to allow indirect rendering
			chunkDrawCommandBuffer.bind();
		}
	}

	// Chunk positions
	{
		const size_t bufferCapacity = drawCount * sizeof(glm::ivec3);
		if (chunkPositionSSBO.getCapacity() < bufferCapacity)
		{
			chunkPositionSSBO.create(GL_SHADER_STORAGE_BUFFER);
			chunkPositionSSBO.allocateStorage(bufferCapacity, GL_DYNAMIC_STORAGE_BIT);

			// Bind SSBO
			chunkPositionSSBO.bindBase(0);
		}
	}
}

void WorldRenderer::passDataToChunkRenderBuffers(size_t drawCount)
{
	// Draw commands
	const size_t drawCommandBufferRequiredCapacity = drawCount * sizeof(DrawArraysIndirectCommand);
	chunkDrawCommandBuffer.write(chunkDrawCommands.data(), drawCommandBufferRequiredCapacity);

	// Chunk positions
	const size_t chunkPositionBufferRequiredCapacity = drawCount * sizeof(glm::ivec3);
	chunkPositionSSBO.write(chunkPositions.data(), chunkPositionBufferRequiredCapacity);
}

void WorldRenderer::renderChunks(const Camera& camera, const FrameBuffer& FBO)
{
	TRACY_SCOPE_NC("Render chunks", ProfileCategory::Render);

	// Set uniforms
	{
		TRACY_SCOPE_NC("Set uniforms", ProfileCategory::Render);

		glm::vec3 fogColor = glm::mix(visualSettings.nightBackgroundColor, visualSettings.dayBackgroundColor, references.dayNightCycleValue);

		const glm::ivec3 cameraChunkPos = glm::ivec3(glm::floor(camera.getPosition())) >> CHUNK_SIZE_LOG2;

		auto viewMatrix = camera.getViewMatrixModified(glm::dvec3(CHUNK_SIZE));
		auto projectionMatrix = camera.getProjectionMatrix();

		const Shader* shaders[] =
		{
			&alignedOpaqueFaceShader,
			&alignedTranslucentFaceShader,
			&unalignedOpaqueFaceShader,
			&unalignedTranslucentFaceShader
		};

		constexpr size_t shaderCount = sizeof(shaders) / sizeof(Shader*);

		for (int i = 0; i < shaderCount; i++)
		{
			const Shader* shader = shaders[i];
			if (!shader->isValid()) continue;

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

	// Collect chunks to render and sort them
	if constexpr (Chunk::USE_CONNECTIVITY_TESTING)
	{
		collectChunksForRenderingWithConnectivity(camera);
	}
	else
	{
		collectChunksForRendering(camera);
	}
	sortChunksForRendering();
	partitionChunksByLayer();

	renderStats.renderedChunkCount = chunksToRender.size();
	renderStats.renderedChunkFaceCount = 0;

	// Bind textures
	if (!Texture::getExtensions().bindless)
	{
		blockTextureArray.bindUnit(0);
	}

	// Set shared opengl states
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	// Render opaque chunks
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	renderChunkGroup<MeshLayer::AlignedOpaque>(alignedOpaqueFaceShader);
	renderChunkGroup<MeshLayer::UnalignedOpaque>(unalignedOpaqueFaceShader);

	// Render translucent chunks
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	renderChunkGroup<MeshLayer::AlignedTranslucent>(alignedTranslucentFaceShader);
	renderChunkGroup<MeshLayer::UnalignedTranslucent>(unalignedTranslucentFaceShader);
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
	// Check if shader is valid
	if (!compositeShader.isValid()) return;

	//
	TRACY_SCOPE_NC("Composite pass", ProfileCategory::Render);

#pragma region GetTextures
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
#pragma endregion

	// Get texture dimensions
	int width = colorTex.getWidth();
	int height = colorTex.getHeight();

	// Bind textures
	glBindImageTexture(0, colorTex.getID(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8); // TODO: Maybe add image handle in future
	if (Texture::getExtensions().bindless)
	{
		compositeShader.setHandleui64ARB("geometryAlphaTex", geometryAlphaTex.getHandle());
		compositeShader.setHandleui64ARB("accumulationTex", accumulationTex.getHandle());
		compositeShader.setHandleui64ARB("revealageTex", revealageTex.getHandle());
		compositeShader.setHandleui64ARB("auroraTex", auroraTex.getHandle());
	}
	else
	{
		geometryAlphaTex.bindUnit(1);
		accumulationTex.bindUnit(2);
		revealageTex.bindUnit(3);
		auroraTex.bindUnit(4);
	}

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
	TRACY_SCOPE_NC("World render", ProfileCategory::Render);

	// Clear buffers
	{
		TRACY_SCOPE_NC("Clear buffers", ProfileCategory::Render);

		//const float color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		//FBO.clearDrawBuffer("color", color);

		const float geometryAlpha[] = { 0.0 };
		const float accumulation[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		const float revealage[] = { 0.0f };
		const float aurora[] = { 0.0f, 0.0f, 0.0f, 0.0f };

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
}

void WorldRenderer::setRenderDistance(int renderDistanceInChunks)
{
	this->chunkRenderDistance = renderDistanceInChunks;

	float fogDistance = (renderDistanceInChunks - 0.5f) * CHUNK_SIZE;

	visualSettings.fogDensity = visualSettings.calculateFogDensity(fogDistance, visualSettings.fogGradient);
}
