#pragma once
#include <cstdint>

struct BlockFaceInstance
{
	uint32_t data1;
	uint32_t data2;

	BlockFaceInstance(uint32_t x, uint32_t y, uint32_t z, uint32_t normal, uint32_t ao, uint32_t textureID, uint32_t light);

	void decodePosition(int& outX, int& outY, int& outZ) const;
};