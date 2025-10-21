#include "MeshData.h"

#include <glm/vec2.hpp>
#include <iostream>

MeshData::MeshData()
{
}

MeshData::~MeshData()
{}

void MeshData::resetFaceCount()
{
	opaqueFaceCount = 0;
	transparentFaceCount = 0;
}