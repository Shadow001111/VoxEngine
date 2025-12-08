#pragma once
#include <cstdint>

struct AlignedBlockFace
{
	// Data1 (32 bits)
	uint32_t x : 4;						// 0-3: X coordinate (0-15)
	uint32_t y : 4;						// 4-7: Y coordinate (0-15)
	uint32_t z : 4;						// 8-11: Z coordinate (0-15)
	uint32_t normal : 3;				// 12-14: Normal
	uint32_t ao : 8;					// 15-22: Ambient occlusion
	uint32_t textureID : 6;				// 23-28: Texture ID
	uint32_t textureTransformation : 3; // 29-31: Texture transformation

	// Data2 (32 bits)
	uint32_t light : 32;				// 0-31: Lighting

	AlignedBlockFace() = default;
	AlignedBlockFace(uint32_t x, uint32_t y, uint32_t z, uint32_t normal, uint32_t ao, uint32_t textureID, uint32_t textureTransformation, uint32_t light);
};