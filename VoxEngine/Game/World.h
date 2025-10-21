#pragma once
#include "World/ChunkPool.h"
#include "World/WorldVisualSettings.h"
#include "World/VoxelMarkerMesh.h"

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
private:
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
public:
	WorldVisualSettings visuals;

	World();
	~World();

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;

	void preparation(int renderDistance);
	void loadChunksAroundPlayer(const glm::vec3& loaderPos, int renderDistance);
	void update();
	void sortChunkMeshes(const glm::vec3& cameraPos);
	void sendChunkMeshesToGPU();

	void renderChunks(const Camera& camera) const;
	void renderVoxelMarker(const Camera& camera, const RaycastResult& raycast) const;

	RaycastResult raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 100.0f) const;

	// Debug
	void rebuildAllChunkMeshes();
	void debugMethod();

	const DebugData& getDebugData() const;
private:
	Chunk* getChunkAt(const glm::ivec3& position) const;

	bool chunkExistsAt(const glm::ivec3& position) const;

	void unloadChunksOutsideRange(int renderDistance);
	void loadChunk(int chunkX, int chunkY, int chunkZ, std::vector<Chunk*>& chunksToSend);

	void startBuildingChunkBlocks();
	void startBuildingChunkLights();
	void startBuildingChunkMeshes();

	void updateChunkLights();
	void collectChunksNeedingLightUpdate();

	void collectChunksToRender(std::vector<ChunkRenderInfo>& chunksToRender, const Camera& camera) const;
};

