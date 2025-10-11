#pragma once
#include <cstdint>

struct BlockFaceInstance
{
	int32_t data;

	BlockFaceInstance(int x, int y, int z, int normal, int ao, int textureID);
};