#include "SphericalChunkLoader.h"
#include <cmath>

std::vector<glm::ivec3> SphericalChunkLoader::getPositionsToLoad(const glm::ivec3& playerChunkPosition, int loadRadius)
{
    std::vector<glm::ivec3> positions;
    {
        int r = loadRadius + 1;
        int approximateSphereVolume = 4 * r * r * r;
        positions.reserve(approximateSphereVolume);
    }

    // Corners
    const int loadRadiusSquared = loadRadius * loadRadius;
    for (int x = 0; x <= loadRadius; x++)
    {
        int D1 = loadRadiusSquared - x * x;
        int yRange = (int)sqrtf(D1);
        for (int y = 0; y <= yRange; y++)
        {
            int D2 = D1 - y * y;
            int zRange = (int)sqrtf(D2);
            for (int z = 0; z <= zRange; z++)
            {
                // Positions can contain duplicates, but that doesn't matter much
                positions.emplace_back(playerChunkPosition.x + x, playerChunkPosition.y + y, playerChunkPosition.z + z);
                positions.emplace_back(playerChunkPosition.x + x, playerChunkPosition.y + y, playerChunkPosition.z - z);
                positions.emplace_back(playerChunkPosition.x + x, playerChunkPosition.y - y, playerChunkPosition.z + z);
                positions.emplace_back(playerChunkPosition.x + x, playerChunkPosition.y - y, playerChunkPosition.z - z);
                positions.emplace_back(playerChunkPosition.x - x, playerChunkPosition.y + y, playerChunkPosition.z + z);
                positions.emplace_back(playerChunkPosition.x - x, playerChunkPosition.y + y, playerChunkPosition.z - z);
                positions.emplace_back(playerChunkPosition.x - x, playerChunkPosition.y - y, playerChunkPosition.z + z);
                positions.emplace_back(playerChunkPosition.x - x, playerChunkPosition.y - y, playerChunkPosition.z - z);
            }
        }
    }

    return positions;
}
