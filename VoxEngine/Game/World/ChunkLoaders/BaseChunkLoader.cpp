// BaseChunkLoader.cpp
#include "BaseChunkLoader.h"
#include "Game/TracyProfiler.h"

void BaseChunkLoader::update(const glm::ivec3& playerChunkPosition, int loadRadius)
{
    TRACY_SCOPE("ChunkLoader update", ProfileCategory::General);

    prevLoaded.swap(loaded);
    loaded.clear();

    chunkLoaderPositions.clear();
    {
        TRACY_SCOPE("Get positions", ProfileCategory::General);
        getPositionsToLoad(playerChunkPosition, loadRadius, chunkLoaderPositions);
    }

    {
        TRACY_SCOPE("Insert positions", ProfileCategory::General);

        const int estimatedRegions = static_cast<int>(chunkLoaderPositions.size()) / Region::REGION_VOLUME + 1;
        loaded.reserve(estimatedRegions);

        for (const auto& pos : chunkLoaderPositions)
        {
            const glm::ivec3 region = Region::transformPositionToRegion(pos);
            loaded[region].setIndex(Region::getIndexFromPosition(pos));
        }
    }

    computeDiffs();
}

void BaseChunkLoader::computeDiffs()
{
    TRACY_SCOPE("Compute differences", ProfileCategory::General);

    toLoad.clear();
    toUnload.clear();

    {
        TRACY_SCOPE("Compute to load", ProfileCategory::General);

        for (const auto& [region, current] : loaded)
        {
            auto it = prevLoaded.find(region);

            if (it == prevLoaded.end())
            {
                current.appendAllPositions(region, toLoad);
            }
            else
            {
                Region::appendPositionsFromBits(region, current.bits() & ~it->second.bits(), toLoad);
            }
        }
    }

    {
        TRACY_SCOPE("Compute to unload", ProfileCategory::General);

        for (const auto& [region, previous] : prevLoaded)
        {
            auto it = loaded.find(region);

            if (it == loaded.end())
            {
                previous.appendAllPositions(region, toUnload);
            }
            else
            {
                Region::appendPositionsFromBits(region, previous.bits() & ~it->second.bits(), toUnload);
            }
        }
    }
}