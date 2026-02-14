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

	LightNode(uint8_t x, uint8_t y, uint8_t z);
};

struct LightRemovalNode
{
	uint8_t x : 4, y : 4, z : 4, lightLevel : 4;

	LightRemovalNode(uint8_t x, uint8_t y, uint8_t z, uint8_t lightLevel);
};

struct LightNodeBulkUpdate
{
	uint8_t x : 4, y : 4, z : 4;
	uint8_t lightToSet : 4;

	LightNodeBulkUpdate(uint8_t x, uint8_t y, uint8_t z, uint8_t lightToSet);
};

//struct LightRemovalNodeBulkUpdate
//{
//	uint8_t x : 4, y : 4, z : 4;
//	uint8_t lightToSet : 4;
//};