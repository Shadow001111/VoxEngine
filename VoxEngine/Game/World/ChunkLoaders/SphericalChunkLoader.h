#pragma once
#include "BaseChunkLoader.h"

class SphericalChunkLoader : public BaseChunkLoader
{
protected:
	std::vector<glm::ivec3> getPositionsToLoad(const glm::ivec3& playerChunkPosition, int loadRadius) override;
};

