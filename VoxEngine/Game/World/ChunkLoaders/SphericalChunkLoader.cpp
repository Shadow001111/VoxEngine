#include "SphericalChunkLoader.h"
#include <cmath>

void SphericalChunkLoader::getPositionsToLoad(const glm::ivec3& playerChunkPosition, int loadRadius, PositionHandler& handler)
{
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
                handler.insert(playerChunkPosition + glm::ivec3{  x,  y,  z });
                handler.insert(playerChunkPosition + glm::ivec3{  x,  y, -z });
                handler.insert(playerChunkPosition + glm::ivec3{  x, -y,  z });
                handler.insert(playerChunkPosition + glm::ivec3{  x, -y, -z });
                handler.insert(playerChunkPosition + glm::ivec3{ -x,  y,  z });
                handler.insert(playerChunkPosition + glm::ivec3{ -x,  y, -z });
                handler.insert(playerChunkPosition + glm::ivec3{ -x, -y,  z });
                handler.insert(playerChunkPosition + glm::ivec3{ -x, -y, -z });
            }
        }
    }
}
