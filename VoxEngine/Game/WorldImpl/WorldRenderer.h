#pragma once
#include "Game/TracyProfiler.h"

#include "OpenGLWrappers/Shader.h"
#include "OpenGLWrappers/ImmutableBuffer.h"
#include "OpenGLWrappers/FrameBuffer.h"

#include "Game/World/VoxelMarkerMesh.h"
#include "Game/World/RaycastResult.h"
#include "Game/World/WorldVisualSettings.h"
#include "Game/World/Chunk/ChunkMesh/ChunkMeshAllocator.h"

#include "Game/ProfileCategories.h"

#include "Graphics/Camera.h"
#include "Graphics/DrawCommands.h"

class Chunk;
class ChunkRegion;

class WorldRenderer
{
	struct ChunkRenderInfo
	{
		const Chunk* chunk;
		unsigned int manhattanDistance;

		ChunkRenderInfo();
		ChunkRenderInfo(const Chunk* chunk, unsigned int manhattanDistance);
	};
public:
	struct RenderStats
	{
		size_t renderedChunkCount = 0;
		size_t renderedChunkFaceCount = 0;

		size_t chunkDrawCommandBufferSizeInBytes = 0;
		size_t chunkPositionBufferSizeInBytes = 0;
	};
private:
	// Types
	using ChunkContainer = robin_hood::unordered_flat_map<glm::ivec3, Chunk*, ivec3Hasher>;
	using ChunkRegionContainer = robin_hood::unordered_flat_map<glm::ivec3, ChunkRegion*, ivec3Hasher>;

	// References
	struct ReferencesFromWorld
	{
		const float& dayNightCycleValue;
		const float& skyLightSub;
		const float& appTime;
	};
	ReferencesFromWorld references;

	// Visual settings
	WorldVisualSettings visualSettings;

	// Other member variables
	int chunkRenderDistance = 0;

	// Render stats
	RenderStats renderStats;

	// Resources
	Shader alignedOpaqueFaceShader;
	Shader alignedTranslucentFaceShader;
	Shader unalignedOpaqueFaceShader;
	Shader unalignedTranslucentFaceShader;
	Shader compositeShader;

	Shader voxelMarkerShader;
	VoxelMarkerMesh voxelMarkerMesh;

	Texture blockTextureArray;

	ImmutableBuffer chunkDrawCommandBuffer;
	ImmutableBuffer chunkPositionSSBO;

	Shader auroraShader;
	ImmutableBuffer skyViewRaysUBO;

	Texture tilingPerlinNoise3DTexture;

	// Render data containers
	mutable std::vector<ChunkRenderInfo> chunksToRender;
	mutable robin_hood::unordered_flat_map<const Chunk*, uint8_t> floodFillVisited;

	mutable std::vector<DrawArraysIndirectCommand> chunkDrawCommands;
	mutable std::vector<glm::ivec3> chunkPositions;

	mutable std::vector<size_t> sortingCountArray;
	mutable std::vector<ChunkRenderInfo> sortingChunkOutputArray;

	// Aurora varaibless
	static constexpr float AURORA_THRESHOLD = 0.02f;
	float auroraAlpha = 0.0f;

	// Implementation/methods

	void initTextures(const std::vector<std::string>& blockTextureNames);
	void initBuffers();
	void initShaders();

	void collectChunksForRendering(const Camera& camera) const;
	void collectChunksForRenderingWithConnectivity(const Camera& camera) const;

	void sortChunksForRendering() const;

	void ensureCapacityForChunkRenderBuffers(size_t drawCount);
	void passDataToChunkRenderBuffers(size_t drawCount);

	template<MeshLayer layer>
	void renderChunkGroup(const Shader& shader);

	void renderChunks(const Camera& camera, const FrameBuffer& FBO);

	void renderAurora(const Camera& camera, const FrameBuffer& FBO) const;

	void compositePass(const FrameBuffer& FBO) const;

	void renderVoxelMarker(const Camera& camera, const RaycastResult& raycast) const;
public:
	WorldRenderer(const ReferencesFromWorld& references);
	~WorldRenderer() = default;

	WorldRenderer(const WorldRenderer&) = delete;
	WorldRenderer& operator=(const WorldRenderer&) = delete;
	WorldRenderer(WorldRenderer&&) = delete;
	WorldRenderer& operator=(WorldRenderer&&) = delete;

	void init(const std::vector<std::string>& blockTextureNames);

	void update();

	void render(const Camera& camera, const FrameBuffer& FBO, const RaycastResult& raycast);

	void setRenderDistance(int renderDistanceInChunks);

	const RenderStats& getRenderStats() const { return renderStats; }
};

template<MeshLayer layer>
inline void WorldRenderer::renderChunkGroup(const Shader& shader)
{
	TRACY_SCOPE_NC("Render chunk group", ProfileCategory::Render);

	// Check if shader is valid
	if (!shader.isValid())
	{
		return;
	}

	// Bind shader for use
	shader.use();

	// Collect draw commands
	size_t drawCount;
	{
		TRACY_SCOPE_NC("Collect draw commands", ProfileCategory::Render);

		const size_t capacity = chunksToRender.size();
		chunkDrawCommands.reserve(capacity);
		chunkPositions.reserve(capacity);

		BufferStreamWriter<DrawArraysIndirectCommand> drawCommandsWriter(chunkDrawCommands.data());
		BufferStreamWriter<glm::ivec3> chunkPositionsWriter(chunkPositions.data());

		for (const auto& info : chunksToRender)
		{
			info.chunk->collectRenderData<layer>(drawCommandsWriter, chunkPositionsWriter);
		}

		drawCount = drawCommandsWriter.getDestination() - chunkDrawCommands.data();
	}
	if (drawCount == 0) return;

	// Collect statistics
	{
		TRACY_SCOPE_NC("Collect statistics", ProfileCategory::Render);
		for (size_t i = 0; i < drawCount; i++)
		{
			renderStats.renderedChunkFaceCount += chunkDrawCommands[i].instanceCount;
		}
	}

	// Pass data to GPU
	ensureCapacityForChunkRenderBuffers(drawCount);
	passDataToChunkRenderBuffers(drawCount);

	// Bind VAO and render
	{
		TRACY_SCOPE_NC("Bind VAO and render", ProfileCategory::Render);
		ChunkMeshAllocator::getInstance().bindVAO(layer);
		glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, (GLsizei)drawCount, 0);
	}
}
