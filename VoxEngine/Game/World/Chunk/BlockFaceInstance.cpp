#include "BlockFaceInstance.h"

BlockFaceInstance::BlockFaceInstance(int x, int y, int z, int normal, int ao, int textureID) : data(0)
{
	// (32 bits left) Coords 12 bits
	data |= (x & 15);
	data |= (y & 15) << 4;
	data |= (z & 15) << 8;

	// (20 bits left) Normal 3 bits
	data |= (normal & 7) << 12;

	// (17 bits left) Ambient occlusion 8 bits
	data |= (ao & 255) << 15;

	// (9 bits left) Texture ID
	data |= (textureID & 511) << 23;

	// (0 bits left)
}