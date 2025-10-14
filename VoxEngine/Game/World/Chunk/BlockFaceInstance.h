#pragma once
#include <cstdint>

struct BlockFaceInstance
{
	int32_t data1;
	int32_t data2;

	BlockFaceInstance(int32_t x, int32_t y, int32_t z, int32_t normal, int32_t ao, int32_t textureID, int32_t light);
};