#pragma once
#include <cstdint>

union LightLevel
{
	struct
	{
		uint8_t blockLight : 4;
		uint8_t skyLight : 4;
	};

	uint8_t fullByte;

	LightLevel();
	LightLevel(uint8_t blockLight, uint8_t skyLight);

	LightLevel(const LightLevel& other);
	LightLevel& operator=(const LightLevel& other);
};

struct LightNode
{
	uint8_t x : 4, y : 4, z : 4;

	LightNode(int x, int y, int z);
};

struct LightRemovalNode
{
	uint8_t x : 4, y : 4, z : 4, lightLevel : 4;

	LightRemovalNode(int x, int y, int z, uint8_t lightLevel);
};