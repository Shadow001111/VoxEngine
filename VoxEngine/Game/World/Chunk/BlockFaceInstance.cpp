#include "BlockFaceInstance.h"

BlockFaceInstance::BlockFaceInstance(int32_t x, int32_t y, int32_t z, int32_t normal, int32_t ao, int32_t textureID, int32_t light) :
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