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
	renderOpaqueFaceCount = 0;
	renderTransparentFaceCount = 0;
}

void MeshData::updateRenderFaceCount()
{
	renderOpaqueFaceCount = getOpaqueFaceCount();
	renderTransparentFaceCount = getTransparentFaceCount();
}
