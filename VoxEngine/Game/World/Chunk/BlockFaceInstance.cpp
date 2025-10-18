#include "BlockFaceInstance.h"

BlockFaceInstance::BlockFaceInstance(uint32_t x, uint32_t y, uint32_t z, uint32_t normal, uint32_t ao, uint32_t textureID, uint32_t light) :
	data1(0), data2(light)
{
	{
		// (32 bits left) Coords 12 bits
		data1 |= (x & 15);
		data1 |= (y & 15) << 4;
		data1 |= (z & 15) << 8;

		// (20 bits left) Normal 3 bits
		data1 |= (normal & 7) << 12;

		// (17 bits left) Ambient occlusion 8 bits
		data1 |= (ao & 255) << 15;

		// (9 bits left) Texture ID
		data1 |= (textureID & 511) << 23;

		// (0 bits left)
	}
}

void BlockFaceInstance::decodePosition(int& outX, int& outY, int& outZ) const
{
	outX = data1 & 15;
	outY = (data1 >> 4) & 15;
	outZ = (data1 >> 8) & 15;
}
