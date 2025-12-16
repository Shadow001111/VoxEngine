#pragma once
#include <cstdint>
#include <string>
#include "../ObjectTypes.h"

struct ItemData
{
	std::string stringId;
	uint8_t stackSize = 0;
	bool hasBlockPlaceable = false;
	BlockId blockPlaceableId;
	TextureId textureId;
};