#include "BaseChunkLoader.h"

void BaseChunkLoader::update(const glm::ivec3& playerChunkPosition, int loadRadius)
{
	prevLoaded.swap(loaded);
	loaded.clear();

	auto poses = getPositionsToLoad(playerChunkPosition, loadRadius);
    loaded.insert(poses.begin(), poses.end());

    computeDiffs();
}

void BaseChunkLoader::computeDiffs()
{
    toLoad.clear();
    for (const auto& pos : loaded)
    {
        if (!prevLoaded.contains(pos))
        {
            toLoad.push_back(pos);
        }
    }

    toUnload.clear();
    for (const auto& pos : prevLoaded)
    {
        if (!loaded.contains(pos))
        {
            toUnload.push_back(pos);
        }
    }
}
