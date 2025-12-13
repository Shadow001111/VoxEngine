#pragma once
#include "Chunk.h"
#include <glm/glm.hpp>

struct RaycastResult
{
	bool hit = false;
	BlockId hitBlock;
	glm::dvec3 hitPosition;
	glm::ivec3 hitBlockPosition;
	Chunk* hitChunk = nullptr;
	int hitNormal = -1;
	float distance = 0.0f;
};