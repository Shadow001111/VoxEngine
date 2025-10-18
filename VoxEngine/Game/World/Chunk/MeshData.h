#pragma once
#include "BlockFaceInstance.h"

#include <glad/glad.h>
#include <cstdint>
#include <vector>

class MeshData
{
	GLuint vao, vbo, instanceVBO;
public:
	uint16_t opaqueFaceCount[6]; // Count for each side
	uint16_t transparentFaceCount[6];
private:
	uint16_t faceCapacity;
public:
	bool ready;
	bool dirty;

	std::vector<BlockFaceInstance> opaqueInstances;
	std::vector<BlockFaceInstance> transparentInstances;

	MeshData();
	~MeshData();

	void resetFaceCount();
	void allocateMemoryForBuffer(size_t faceCount);
	void bindVAO() const;
	void bindInstanceVBO() const;

	size_t getOpaqueFaceCountSum() const;
	size_t getTransparentFaceCountSum() const;
	size_t getFaceCapacity() const;
};