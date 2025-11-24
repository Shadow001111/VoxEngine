#include "AlignedBlockFace.h"

AlignedBlockFace::AlignedBlockFace(uint32_t x, uint32_t y, uint32_t z, uint32_t normal, uint32_t ao, uint32_t textureID, uint32_t textureTransformation, uint32_t light) :
	x(x), y(y), z(z),
	normal(normal),
	ao(ao),
	textureID(textureID),
	textureTransformation(textureTransformation),
	light(light)
{}
