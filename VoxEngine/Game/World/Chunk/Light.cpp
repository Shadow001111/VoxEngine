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


LightNode::LightNode(int x, int y, int z) :
	x(x), y(y), z(z)
{
}


LightRemovalNode::LightRemovalNode(int x, int y, int z, uint8_t lightLevel) :
	x(x), y(y), z(z), lightLevel(lightLevel)
{
}