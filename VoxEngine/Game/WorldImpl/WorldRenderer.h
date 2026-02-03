#pragma once
#include "OpenGLWrappers/Shader.h"
#include "OpenGLWrappers/Buffer.h"
#include "OpenGLWrappers/ImmutableBuffer.h"
#include "OpenGLWrappers/FrameBuffer.h"

#include "../World/VoxelMarkerMesh.h"
#include "../World/RaycastResult.h"
#include "../World/WorldVisualSettings.h"

#include "../World/Chunk/ChunkNormalPacker.h"

#include "Graphics/Camera.h"

class Chunk;

class WorldRenderer
{
	struct ChunkRenderInfo
	{
		const Chunk* chunk;
		unsigned int manhattanDistance;

		ChunkRenderInfo(const Chunk* chunk, unsigned int manhattanDistance);
	};
public:
	struct RenderStats
	{
		size_t renderedChunks = 0;
		size_t renderedChunkFaceCount = 0;

		size_t chunkDrawCommandBufferSizeInBytes = 0;
		size_t chunkPositionBufferSizeInBytes = 0;
		size_t chunkPositionIndexBufferSizeInBytes = 0;
		size_t chunkNormalBufferSizeInBytes = 0;
	};
private:
	// Types
	using ChunkContainer = robin_hood::unordered_flat_map<glm::ivec3, Chunk*, ivec3Hasher>;

	// References
	struct ReferencesFromWorld
	{
		const ChunkContainer& chunks;
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

	Buffer chunkDrawCommandBuffer;
	Buffer chunkPositionSSBO;
	Buffer chunkPositionIndexSSBO;
	Buffer chunkNormalSSBO;

	Shader auroraShader;
	ImmutableBuffer skyViewRaysUBO;

	Texture tilingPerlinNoise3DTexture;

	// Render data containers
	mutable std::vector<ChunkRenderInfo> chunksToRender;
	mutable std::vector<DrawArraysIndirectCommand> chunkDrawCommands;
	mutable std::vector<glm::ivec3> chunkPositions;
	mutable std::vector<uint32_t> chunkPositionIndices;
	mutable ChunkNormalPacker chunkNormalPacker;

	// Aurora varaibless
	static constexpr float AURORA_THRESHOLD = 0.02f;
	float auroraAlpha = 0.0f;

	// Implementation/methods

	void initTextures(const std::vector<std::string>& blockTextureNames);
	void initBuffers();
	void initShaders();

	void collectAndSortChunksForRendering(const Camera& camera) const;

	void renderOpaqueChunks();
	void renderTranslucentChunks();
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

