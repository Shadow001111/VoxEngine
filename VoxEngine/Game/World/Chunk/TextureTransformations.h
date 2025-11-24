#pragma once
#include <cstdint>

enum class TextureTransformation : uint8_t
{
	None = 0,
	Flip = 1,
	RotateAndFlip = 2
};