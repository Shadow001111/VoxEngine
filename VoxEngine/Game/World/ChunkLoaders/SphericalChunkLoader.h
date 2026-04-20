#pragma once
#include "BaseChunkLoader.h"

class SphericalChunkLoader : public BaseChunkLoader
{
protected:
	void getPositionsToLoad(const glm::ivec3& playerChunkPosition, int loadRadius, PositionHandler& handler) override;
};

