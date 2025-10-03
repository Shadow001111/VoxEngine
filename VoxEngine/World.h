#pragma once
#include "Chunk.h"

#include "Graphics/Camera.h"
#include "Graphics/Shader.h"

#include <unordered_map>

class World
{
	std::unordered_map<Int3, std::unique_ptr<Chunk>, ChunkHash> chunks;

	Int3 lastChunkLoaderPos;
	bool firstLoad = true;
public:
	World();
	~World();

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;

	void loadChunks(const Int3& chunkLoaderPos, int renderDistance);
	void render(const Shader& faceShader) const;
private:
	void loadChunk(int chunkX, int chunkY, int chunkZ);
};

