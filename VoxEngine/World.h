#pragma once
#include "Chunk.h"

#include "Graphics/Shader.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <mutex>

class World
{
	class ChunkPool
	{
		std::vector<std::unique_ptr<Chunk>> pool;
	public:
		ChunkPool() = default;
		~ChunkPool() = default;

		ChunkPool(const ChunkPool&) = delete;
		ChunkPool& operator=(const ChunkPool&) = delete;
		ChunkPool(ChunkPool&&) = delete;
		ChunkPool& operator=(ChunkPool&&) = delete;

		std::unique_ptr<Chunk> acquire();

		void release(std::unique_ptr<Chunk> chunk);
	};

	// TODO: Add ability for
	struct Visuals
	{
		glm::vec3 backgroundColor = {}; // Also fog color
		float fogMaxDistance = 0.0f; // Should be set as render distance
		float fogDensity = 0.0f;
		float fogGradient = 0.0f;

		static float calculateFogDensity(float renderDistance_, float fogGradient_);
		static float calculateFogGradient(float renderDistance_, float fogDensity_);
	};

	ChunkPool chunkPool;
	std::unordered_map<Int3, std::unique_ptr<Chunk>, Int3Hasher> chunks;
	
	std::mutex blocksBuildMutex;
	std::unordered_set<Chunk*> blocksBuildChunkContainer;
	
	std::mutex meshBuildMutex;
	std::unordered_set<Chunk*> meshBuildChunkContainer;

	Int3 lastChunkLoaderPos;
	bool firstLoad = true;
public:
	Visuals visuals;

	World();
	~World();

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;

	void loadChunksAroundPlayer(const Int3& chunkLoaderPos, int renderDistance);
	void update();
	void render(const Shader& faceShader) const;

	// Debug
	void rebuildAllChunkMeshes();
	void debugMethod();

	void getChunkMeshesInfo(size_t& totalFaces, size_t& totalFaceCapacity, size_t& potentialMaximumCapacity);
private:
	Chunk* getChunkAt(const Int3& position) const;
	Chunk* getChunkAt(int x, int y, int z) const;

	bool chunkExistsAt(const Int3& position) const;
	bool chunkExistsAt(int x, int y, int z) const;

	void unloadChunksOutsideRange(int renderDistance);
	void loadChunk(int chunkX, int chunkY, int chunkZ, std::vector<Chunk*>& chunksToSend);

	void startBuildingChunkBlocks();
	void startBuildingChunkMeshes();
};

