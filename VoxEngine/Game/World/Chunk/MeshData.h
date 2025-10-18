#pragma once
#include "BlockFaceInstance.h"

#include <glad/glad.h>
#include <cstdint>
#include <vector>

class MeshData
{
	GLuint vao = 0, vbo = 0, instanceVBO = 0;
public:
	uint16_t opaqueFaceCount[6]; // Count for each side
	uint16_t transparentFaceCount[6];
private:
	uint16_t faceCapacity = 0;
public:
	bool ready = false;
	bool opaqueDirty = false;
	bool transparentDirty = false;

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