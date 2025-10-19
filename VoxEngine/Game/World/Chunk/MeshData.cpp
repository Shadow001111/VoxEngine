#include "MeshData.h"

#include <glm/vec2.hpp>
#include <iostream>

MeshData::MeshData()
{
	resetFaceCount();
}

MeshData::~MeshData()
{}

void MeshData::resetFaceCount()
{
	for (int i = 0; i < 6; i++)
	{
		opaqueFaceCount[i] = 0;
		transparentFaceCount[i] = 0;
	}
}

size_t MeshData::getOpaqueFaceCount() const
{
	size_t sum = 0;
	for (int i = 0; i < 6; i++)
	{
		sum += opaqueFaceCount[i];
	}
	return sum;
}

size_t MeshData::getTransparentFaceCount() const
{
	size_t sum = 0;
	for (int i = 0; i < 6; i++)
	{
		sum += transparentFaceCount[i];
	}
	return sum;
}

size_t MeshData::getFaceCount() const
{
	size_t sum = 0;
	for (int i = 0; i < 6; i++)
	{
		sum += opaqueFaceCount[i];
	}
	for (int i = 0; i < 6; i++)
	{
		sum += transparentFaceCount[i];
	}
	return sum;
}

size_t MeshData::getFaceCapacity() const
{
	return allocatedBlock.size;
}