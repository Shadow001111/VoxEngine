#pragma once
#include "World/ChunkPool.h"
#include "World/WorldVisualSettings.h"
#include "World/VoxelMarkerMesh.h"
#include "World/Entity.h"

#include "Graphics/Shader.h"
#include "Graphics/Camera.h"
#include "Graphics/BlockTextureArray.h"
#include "Graphics/OpenGL_SSBO.h"

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
	struct RaycastResult
	{
		bool hit = false;
		Block hitBlock = Block::Air;
		glm::vec3 hitPosition;
		glm::ivec3 hitBlockPosition;
		Chunk* hitChunk = nullptr;
		int hitNormal = -1;
		float distance = 0.0f;
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
	std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, Int3Hasher> chunks;
	
	std::unordered_set<Chunk*> buildBlocksContainer;
	std::mutex buildBlocksMutex;

	std::unordered_set<Chunk*> buildLightContainer;
	std::mutex buildLightMutex;
	
	std::unordered_set<Chunk*> buildMeshContainer;
	std::mutex buildMeshMutex;

	std::unordered_set<Chunk*> lightUpdateContainer;

	glm::ivec3 lastChunkLoaderPos = { INT_MAX, INT_MAX, INT_MAX };
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
	void loadChunksAroundPlayer(const glm::vec3& loaderPos);
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

		auto e = std::make_unique<T>(std::forward<Args>(args)...);
		T* raw = e.get();
		entities.emplace(e->getId(), std::move(e));
		return raw;
	}

	// Debug
	void rebuildAllChunkMeshes();
	void debugMethod();

	const DebugData& getDebugData() const;
private:
	Chunk* getChunkAt(const glm::ivec3& position) const;

	bool chunkExistsAt(const glm::ivec3& position) const;

	void unloadChunksOutsideRange();
	void loadChunk(int chunkX, int chunkY, int chunkZ, std::vector<Chunk*>& chunksToSend);

	void startBuildingChunkBlocks();
	void startBuildingChunkLights();
	void startBuildingChunkMeshes();

	void updateChunkLights();
	void collectChunksNeedingLightUpdate();

	void collectChunksToRender(std::vector<ChunkRenderInfo>& chunksToRender, const Camera& camera) const;
public:
	bool placeBlock(const RaycastResult& raycast, Block block);
	bool breakBlock(const RaycastResult& raycast);
	void updateBlockAt(const glm::ivec3& worldPos, Block block);
public:
	void setChunkLoadingDistance(int renderDistance);
public:
	std::optional<Block> getBlockAt(const glm::ivec3& globalPosition) const;
};

