#include "BaseChunkLoader.h"
#include "Game/TracyProfiler.h"

void BaseChunkLoader::update(const glm::ivec3& playerChunkPosition, int loadRadius)
{
	prevLoaded.swap(loaded);
	loaded.clear();

    chunkLoaderPositions.clear();
    {
        TRACY_SCOPE("Get positions", ProfileCategory::General);
        getPositionsToLoad(playerChunkPosition, loadRadius, chunkLoaderPositions);
    }
    {
        TRACY_SCOPE("Insert positions", ProfileCategory::General);
        loaded.reserve(chunkLoaderPositions.size());
        loaded.insert(chunkLoaderPositions.begin(), chunkLoaderPositions.end());
    }

    computeDiffs();
}

void BaseChunkLoader::computeDiffs()
{
    TRACY_SCOPE("Compute differences", ProfileCategory::General);

    toLoad.clear();
    toLoad.reserve(loaded.size());
    for (const auto& pos : loaded)
    {
        if (prevLoaded.find(pos) == prevLoaded.end())
        {
            toLoad.push_back(pos);
        }
    }

    toUnload.clear();
    toUnload.reserve(prevLoaded.size());
    for (const auto& pos : prevLoaded)
    {
        if (loaded.find(pos) == loaded.end())
        {
            toUnload.push_back(pos);
        }
    }
}
