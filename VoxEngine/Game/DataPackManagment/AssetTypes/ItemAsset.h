#pragma once
#include <cstdint>
#include <string>

struct ItemAsset
{
	std::string stringId;
	uint8_t stackSize = 0;
	bool hasBlockPlaceable = false;
	std::string blockPlaceableStringId;
	std::string uiTextureName;
};