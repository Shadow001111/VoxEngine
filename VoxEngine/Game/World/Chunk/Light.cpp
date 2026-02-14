#include "Light.h"

LightLevel::LightLevel() :
	fullByte(0)
{}

LightLevel::LightLevel(uint8_t blockLight, uint8_t skyLight) :
	blockLight(blockLight), skyLight(skyLight)
{}

LightLevel::LightLevel(const LightLevel& other) :
	fullByte(other.fullByte)
{}

LightLevel& LightLevel::operator=(const LightLevel& other)
{
	fullByte = other.fullByte;
	return *this;
}


LightNode::LightNode(uint8_t x, uint8_t y, uint8_t z) :
	x(x), y(y), z(z)
{}


LightRemovalNode::LightRemovalNode(uint8_t x, uint8_t y, uint8_t z, uint8_t lightLevel) :
	x(x), y(y), z(z), lightLevel(lightLevel)
{}

LightNodeBulkUpdate::LightNodeBulkUpdate(uint8_t x, uint8_t y, uint8_t z, uint8_t lightToSet) :
	x(x), y(y), z(z), lightToSet(lightToSet)
{}
