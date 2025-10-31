#pragma once
#include <cstdint>

enum class Block : uint8_t
{
	Air = 0,
	GrassBlock,
	Dirt,
	Stone,
	//GlowStone,
	Glass,
	ColoredGlass,
	__BlockCount__
};
