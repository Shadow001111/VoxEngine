#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "robin_hood.h"

#include "Core/Hashes/ivec3Hasher.h"

class BaseChunkLoader
{
    using ChunkSet = robin_hood::unordered_flat_set<glm::ivec3, ivec3Hasher>;

    std::vector<glm::ivec3> chunkLoaderPositions;
    ChunkSet loaded;
    ChunkSet prevLoaded;
    std::vector<glm::ivec3> toLoad;
    std::vector<glm::ivec3> toUnload;
public:
	virtual ~BaseChunkLoader() = default;

	void update(const glm::ivec3& playerChunkPosition, int loadRadius);

    const std::vector<glm::ivec3>& getChunksToLoad() const noexcept { return toLoad; }
    const std::vector<glm::ivec3>& getChunksToUnload() const noexcept { return toUnload; }
protected:
    virtual void getPositionsToLoad(const glm::ivec3& playerChunkPosition, int loadRadius, std::vector<glm::ivec3>& positions) = 0;
private:
    void computeDiffs();
};

