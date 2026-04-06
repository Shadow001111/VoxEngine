#pragma once
#include "Core/Profiler.h"

#include "OpenGLWrappers/Shader.h"
#include "OpenGLWrappers/ImmutableBuffer.h"
#include "OpenGLWrappers/FrameBuffer.h"

#include "Game/World/VoxelMarkerMesh.h"
#include "Game/World/RaycastResult.h"
#include "Game/World/WorldVisualSettings.h"
#include "Game/World/Chunk/ChunkMesh/ChunkMeshAllocator.h"

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

	using ChunkCollectFunc = void (Chunk::*)(BufferStreamWriter<DrawArraysIndirectCommand>&, BufferStreamWriter<glm::ivec3>&) const;
	using ChunkInstancedMeshAllocatorBindVAOFunc = void (ChunkMeshAllocator::*)() const;
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
	Shader nonAlignedOpaqueFaceShader;
	Shader nonAlignedTranslucentFaceShader;
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

	void sortChunksForRendering() const;

	void ensureCapacityForChunkRenderBuffers(size_t drawCount);
	void passDataToChunkRenderBuffers(size_t drawCount);

	template<ChunkCollectFunc CollectMethod, ChunkInstancedMeshAllocatorBindVAOFunc BindVAOMethod>
	void renderChunksGeneral(const Shader& shader);

	void renderAlignedOpaqueChunks();
	void renderNonAlignedOpaqueChunks();
	void renderAlignedTranslucentChunks();
	void renderNonAlignedTranslucentChunks();

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

template<WorldRenderer::ChunkCollectFunc CollectMethod, WorldRenderer::ChunkInstancedMeshAllocatorBindVAOFunc BindVAOMethod>
inline void WorldRenderer::renderChunksGeneral(const Shader& shader)
{
	size_t drawCount;
	{
		PROFILE_SCOPE("Render: collect draw commands", ProfileCategory::Render);

		const size_t capacity = chunksToRender.size();
		chunkDrawCommands.reserve(capacity);
		chunkPositions.reserve(capacity);

		BufferStreamWriter<DrawArraysIndirectCommand> drawCommandsWriter(chunkDrawCommands.data());
		BufferStreamWriter<glm::ivec3> chunkPositionsWriter(chunkPositions.data());

		for (const auto& info : chunksToRender)
		{
			(info.chunk->*CollectMethod)(drawCommandsWriter, chunkPositionsWriter);
		}

		drawCount = drawCommandsWriter.getDestination() - chunkDrawCommands.data();
	}

	if (drawCount == 0)
	{
		return;
	}

	for (size_t i = 0; i < drawCount; i++)
	{
		renderStats.renderedChunkFaceCount += chunkDrawCommands[i].instanceCount;
	}

	ensureCapacityForChunkRenderBuffers(drawCount);
	passDataToChunkRenderBuffers(drawCount);

	shader.use();
	(ChunkMeshAllocator::getInstance().*BindVAOMethod)();
	glMultiDrawArraysIndirect(GL_TRIANGLE_FAN, NULL, (GLsizei)drawCount, 0);
}
