#include "BaseChunkLoader.h"
#include "Game/TracyProfiler.h"

void BaseChunkLoader::update(const glm::ivec3& playerChunkPosition, int loadRadius)
{
    TRACY_SCOPE("ChunkLoader update", ProfileCategory::General);

    prevLoaded.swap(loaded);

    loaded.clear();

    {
        TRACY_SCOPE("Get positions", ProfileCategory::General);
        PositionHandler handler(loaded);
        getPositionsToLoad(playerChunkPosition, loadRadius, handler);
    }

    computeDiffs();
}

void BaseChunkLoader::computeDiffs()
{
    TRACY_SCOPE("Compute differences", ProfileCategory::General);

    toLoad.clear();
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

    toUnload.clear();
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

void BaseChunkLoader::Region::appendPositionsFromBits(const glm::ivec3& region, const Bitset& bits, std::vector<glm::ivec3>& out)
{
    for (size_t i = bits.findFirst(); i < REGION_VOLUME; i = bits.findNext(i))
    {
        out.push_back(getPositionFromIndex(region, static_cast<int>(i)));
    }
}