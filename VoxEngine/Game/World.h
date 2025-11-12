#pragma once
#include "World/ChunkPool.h"
#include "World/WorldVisualSettings.h"
#include "World/VoxelMarkerMesh.h"
#include "World/Entity.h"
#include "World/RaycastResult.h"
#include "World/ChunkLoaders/SphericalChunkLoader.h"

#include "Graphics/Shader.h"
#include "Graphics/Camera.h"
#include "Graphics/BlockTextureArray.h"
#include "Graphics/OpenGL_SSBO.h"

#include "Core/Hashes/ivec3Hasher.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <mutex>

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
		size_t totalFaceCapacity = 0;
		size_t renderedFaceCount = 0;

		size_t chunkMeshesGaps = 0;

		size_t chunkDrawCommandBufferSizeInBytes = 0;
		size_t chunkPositionBufferSizeInBytes = 0;
	};
private:
	// Settings
	int chunkLoadingDistance = 0;

	//
	ChunkPool chunkPool;
	std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, ivec3Hasher> chunks;
	
	std::unordered_set<Chunk*> buildBlocksContainer;
	std::mutex buildBlocksMutex;

	std::unordered_set<Chunk*> buildLightContainer;
	std::mutex buildLightMutex;

	std::vector<Chunk*> lightUpdateContainer;

	glm::ivec3 lastChunkLoaderPos = { INT_MAX, INT_MAX, INT_MAX };
	int lastChunkLoadingDistance = -1;

	glm::ivec3 lastChunkMeshSortPos = { INT_MAX, INT_MAX, INT_MAX };

	// Resources
	std::unique_ptr<Shader> faceShader;

	std::unique_ptr<Shader> voxelMarkerShader;
	VoxelMarkerMesh voxelMarkerMesh;

	std::unique_ptr<BlockTextureArray> blockTextureArray;

	std::unique_ptr<OpenGL_Buffer> chunkDrawCommandBuffer;
	std::unique_ptr<OpenGL_SSBO> chunkPositionSSBO;

	// Debug
	mutable DebugData debugData;

	// Visual settings
	WorldVisualSettings visualSettings;

	// Chunk loaders
	std::vector<std::unique_ptr<BaseChunkLoader>> chunkLoaders;

	// Entities
	std::unordered_map<Entity::Id, std::unique_ptr<Entity>> entities;
public:
	World();
	~World();

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;

	void preparation();
	void loadChunks(const glm::vec3& playerPos);
	void update(float deltaTime);
	void sortChunkMeshes(const glm::vec3& cameraPos);
	void sendChunkMeshesToGPU();

	void clearFrambuffer() const;
	void renderChunks(const Camera& camera) const;
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

	void updateChunkLights();
	void collectChunksNeedingLightUpdate();

	void updateChunkMeshes();

	void collectChunksToRender(std::vector<ChunkRenderInfo>& chunksToRender, const Camera& camera) const;
public:
	bool placeBlock(const RaycastResult& raycast, Block block);
	bool breakBlock(const RaycastResult& raycast);
	void updateBlockAt(const glm::ivec3& worldPos, Block block);
public:
	const WorldVisualSettings& getWorldVisualSettings() const;
	void setChunkLoadingDistance(int renderDistance);
public:
	std::optional<Block> getBlockAt(const glm::ivec3& globalPosition) const;
};

