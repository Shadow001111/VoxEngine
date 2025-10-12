#pragma once
#include <cstdint>

struct BlockFaceInstance
{
	int32_t data1;
	int32_t data2;

	// Either I store blockLight and skyLight. Or store maximum of two and update meshes each time global sky light changes.

	BlockFaceInstance(int32_t x, int32_t y, int32_t z, int32_t normal, int32_t ao, int32_t textureID, int32_t light);
};