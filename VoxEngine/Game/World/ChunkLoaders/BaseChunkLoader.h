#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "robin_hood.h"

#include "Core/Hashes/ivec3Hasher.h"

class BaseChunkLoader
{
    using ChunkSet = robin_hood::unordered_flat_set<glm::ivec3, ivec3Hasher>;

    ChunkSet loaded;
    ChunkSet prevLoaded;
    std::vector<glm::ivec3> toLoad;
    std::vector<glm::ivec3> toUnload;
public:
	virtual ~BaseChunkLoader() = default;

	void update(const glm::ivec3& playerChunkPosition, int loadRadius);

    std::vector<glm::ivec3> getChunksToLoad() const { return toLoad; }
    std::vector<glm::ivec3> getChunksToUnload() const { return toUnload; }
protected:
    virtual std::vector<glm::ivec3> getPositionsToLoad(const glm::ivec3& playerChunkPosition, int loadRadius) = 0;
private:
    void computeDiffs();
};

