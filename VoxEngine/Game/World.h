#pragma once
#include "World/ChunkPool.h"
#include "World/WorldVisualSettings.h"

#include "Graphics/Shader.h"
#include "Graphics/Camera.h"
#include "Graphics/BlockTextureArray.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <mutex>

class World
{
	struct ChunkRenderInfo
	{
		const Chunk* chunk;
		glm::vec3 chunkWorldPosition;
		float distanceSquared;

		ChunkRenderInfo(const Chunk* chunk, const glm::vec3& chunkWorldPosition, float distanceSquared);
	};

	ChunkPool chunkPool;
	std::unordered_map<Int3, std::unique_ptr<Chunk>, Int3Hasher> chunks;
	
	std::unordered_set<Chunk*> buildBlocksContainer;
	std::mutex buildBlocksMutex;

	std::unordered_set<Chunk*> buildLightContainer;
	std::mutex buildLightMutex;
	
	std::unordered_set<Chunk*> buildMeshContainer;
	std::mutex buildMeshMutex;

	std::unordered_set<Chunk*> lightUpdateContainer;

	Int3 lastChunkLoaderPos;
	bool firstLoad = true;

	// Resources
	std::unique_ptr<Shader> faceShader;
	std::unique_ptr <BlockTextureArray> blockTextureArray;

	// Debug
	mutable size_t renderedFaceCount;
public:
	WorldVisualSettings visuals;

	World();
	~World();

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;

	void preparation(int renderDistance);
	void loadChunksAroundPlayer(const Int3& chunkLoaderPos, int renderDistance);
	void update();
	void render(const Camera& camera) const;

	// Debug
	void rebuildAllChunkMeshes();
	void debugMethod();

	void getChunkMeshesInfo(size_t& totalFaces, size_t& totalFaceCapacity, size_t& potentialMaximumCapacity, size_t& renderedFaceCount);
private:
	Chunk* getChunkAt(const Int3& position) const;
	Chunk* getChunkAt(int x, int y, int z) const;

	bool chunkExistsAt(const Int3& position) const;
	bool chunkExistsAt(int x, int y, int z) const;

	void unloadChunksOutsideRange(int renderDistance);
	void loadChunk(int chunkX, int chunkY, int chunkZ, std::vector<Chunk*>& chunksToSend);

	void startBuildingChunkBlocks();
	void startBuildingChunkLights();
	void startBuildingChunkMeshes();

	void updateChunkLights();
	void collectChunksNeedingLightUpdate();

	void collectChunksToRender(std::vector<ChunkRenderInfo>& chunksToRender, const Camera& camera) const;
};

