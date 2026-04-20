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
                for (int sx = -1; sx <= 1; sx += 2)
                for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                {
                    int nx = playerChunkPosition.x + sx * x;
                    int ny = playerChunkPosition.y + sy * y;
                    int nz = playerChunkPosition.z + sz * z;

                    // Avoid duplicates
                    if ((x == 0 && sx == -1) ||
                        (y == 0 && sy == -1) ||
                        (z == 0 && sz == -1))
                        continue;

                    handler.insert({nx, ny, nz});
                }
            }
        }
    }
}
