#pragma once
#include "Chunk.h"

#include "Graphics/Shader.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <mutex>
#include <atomic>

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

	ChunkPool chunkPool;
	std::unordered_map<Int3, std::unique_ptr<Chunk>, Int3Hasher> chunks;
	
	std::mutex blocksBuildMutex;
	std::unordered_set<Chunk*> blocksBuildChunkContainer;
	
	std::mutex meshBuildMutex;
	std::unordered_set<Chunk*> meshBuildChunkContainer;

	Int3 lastChunkLoaderPos;
	bool firstLoad = true;
public:
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
	void loadChunk(int chunkX, int chunkY, int chunkZ);

	void startBuildingChunkBlocks();
	void buildChunkMeshes();
};

