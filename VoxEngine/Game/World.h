#pragma once
#include "World/ChunkPool.h"
#include "World/WorldVisualSettings.h"
#include "World/VoxelMarkerMesh.h"
#include "World/Entity.h"
#include "World/RaycastResult.h"
#include "World/ChunkLoaders/SphericalChunkLoader.h"

#include "Graphics/Camera.h"

#include "OpenGLWrappers/Shader.h"
#include "OpenGLWrappers/OpenGL_Buffer.h"
#include "OpenGLWrappers/OpenGL_VAO.h"
#include "OpenGLWrappers/OpenGL_FBO.h"

#include "Core/Hashes/ivec3Hasher.h"

#include <memory>
#include <mutex>
#include "robin_hood.h"

class World
{
	struct ChunkRenderInfo
	{
		const Chunk* chunk;
		unsigned int manhattanDistance;

		ChunkRenderInfo(const Chunk* chunk, unsigned int manhattanDistance);
	};
public:
	struct DebugData
	{
		size_t loadedChunksCount = 0;
		size_t renderedChunks = 0;

		size_t totalFaces = 0;
		size_t totalFaceCapacityInBytes = 0;
		size_t renderedFaceCount = 0;

		size_t chunkDrawCommandBufferSizeInBytes = 0;
		size_t chunkPositionBufferSizeInBytes = 0;
	};
private:
	// Settings
	int chunkLoadingDistance = 0;

	//
	ChunkPool chunkPool;
	robin_hood::unordered_flat_map<glm::ivec3, std::unique_ptr<Chunk>, ivec3Hasher> chunks;
	
	robin_hood::unordered_flat_set<Chunk*> buildBlocksContainer;
	std::mutex buildBlocksMutex;

	robin_hood::unordered_flat_set<Chunk*> buildLightContainer;
	std::mutex buildLightMutex;

	std::vector<Chunk*> lightUpdateContainerA;
	std::vector<Chunk*> lightUpdateContainerB;

	glm::ivec3 lastChunkLoaderPos = { INT_MAX, INT_MAX, INT_MAX };
	int lastChunkLoadingDistance = -1;

	// Resources
	Shader alignedOpaqueFaceShader;
	Shader alignedTranslucentFaceShader;
	Shader nonAlignedOpaqueFaceShader;
	Shader nonAlignedTranslucentFaceShader;
	Shader compositeFaceShader;

	Shader voxelMarkerShader;
	VoxelMarkerMesh voxelMarkerMesh;

	OpenGL_Texture blockTextureArray;

	OpenGL_Buffer chunkDrawCommandBuffer;
	OpenGL_Buffer chunkPositionSSBO;

	Shader skyShader;
	OpenGL_Buffer skyViewRaysUBO;
	OpenGL_Buffer skyAuroraSettingsUBO;

	OpenGL_Texture tilingPerlinNoise3DTexture;

	// Debug
	mutable DebugData debugData;

	// Visual settings
	WorldVisualSettings visualSettings;

	// Chunk loaders
	std::vector<std::unique_ptr<BaseChunkLoader>> chunkLoaders;

	// Entities
	robin_hood::unordered_flat_map<Entity::Id, std::unique_ptr<Entity>> entities;

	// Time
	float appTime = 0.0;
	size_t worldTime = 0;
	float dayNightCycleValue = 0.0f; // 1 day; 0 night
	float skyLightSub = 0.0f;
	static constexpr int TICKS_PER_24_HOURS = 240;// 24000;
public:
	World();
	~World();

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;
public:
	void preparation();
	void loadChunks(const glm::vec3& playerPos);
	void update(float deltaTime);
	void sendChunkMeshesToGPU();
private:
	void collectChunksToRenderAndSortThem(std::vector<ChunkRenderInfo>& chunksToRender, const Camera& camera) const;
public:
	void renderBackround(const Camera& camera, const OpenGL_FBO& opaqueFBO) const;
	void renderAurora(const Camera& camera, const OpenGL_FBO& opaqueFBO) const;

	void renderChunks(const Camera& camera, const OpenGL_FBO& opaqueFBO, const OpenGL_FBO& translucentFBO);
private:
	void renderOpaqueChunks(const std::vector<ChunkRenderInfo>& chunksToRender,
		std::vector<DrawArraysIndirectCommand>& chunkDrawCommands,
		std::vector<glm::ivec3>& chunkPositions);
	void renderTranslucentChunks(const std::vector<ChunkRenderInfo>& chunksToRender,
		std::vector<DrawArraysIndirectCommand>& chunkDrawCommands,
		std::vector<glm::ivec3>& chunkPositions);
	void compositePass(const OpenGL_FBO& opaqueFBO, const OpenGL_FBO& translucentFBO) const;
public:
	void renderVoxelMarker(const Camera& camera, const RaycastResult& raycast) const;

	RaycastResult raycast(const glm::dvec3& origin, const glm::dvec3& direction, float maxDistance = 100.0f) const;

	template<typename T, typename... Args>
	T* createEntity(Args&&... args) {
		static_assert(std::is_base_of<Entity, T>::value, "T must derive from Entity");
		static_assert(!std::is_same_v<Entity, T>, "T mustn't be Entity");

		std::unique_ptr<T> e = std::make_unique<T>(std::forward<Args>(args)...);
		T* raw = e.get();
		entities.emplace(e->getId(), std::move(e));
		return raw;
	}

	// Debug
	void rebuildAllChunkMeshes();
	void debugMethod();

	const DebugData& getDebugData() const;
private:
	template<typename T, typename... Args>
	T* createChunkLoader(Args&&... args)
	{
		static_assert(std::is_base_of<BaseChunkLoader, T>::value, "T must derive from BaseChunkLoader");
		static_assert(!std::is_same_v<BaseChunkLoader, T>, "T mustn't be BaseChunkLoader");

		std::unique_ptr<T> chl = std::make_unique<T>(std::forward<Args>(args)...);
		T* raw = chl.get();
		chunkLoaders.push_back(std::move(chl));
		return raw;
	}
private:
	Chunk* getChunkAt(const glm::ivec3& position) const;

	bool chunkExistsAt(const glm::ivec3& position) const;

	void loadChunk(const glm::ivec3& position);
	void unloadChunk(const glm::ivec3& position);

	void startBuildingChunkBlocks();
	void startBuildingChunkLights();

	void collectChunksNeedingLightUpdate();
	void updateChunkLights();

	void updateChunkMeshes();
public:
	bool placeBlock(const RaycastResult& raycast, BlockId block);
	bool breakBlock(const RaycastResult& raycast);
	void updateBlockAt(const glm::ivec3& worldPos, BlockId block);
public:
	const WorldVisualSettings& getWorldVisualSettings() const;
	void setChunkLoadingDistance(int renderDistance);
public:
	std::optional<BlockId> getBlockAt(const glm::ivec3& globalPosition) const;

	void setAppTime(float time);
};

