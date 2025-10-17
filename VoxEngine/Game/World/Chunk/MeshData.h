#pragma once
#include "BlockFaceInstance.h"

#include "Core/Multithreading/ProcessingFence.h"

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

	std::vector<BlockFaceInstance> opaqueInstances;
	std::vector<BlockFaceInstance> transparentInstances;

	ProcessingFence processingFence;

	MeshData();
	~MeshData();

	void resetFaceCount();
	void allocateMemoryForBuffer(size_t faceCount);
	void bindVAO() const;
	void bindInstanceVBO() const;

	size_t getOpaqueFaceCountSum() const;
	size_t getTransparentFaceCountSum() const;
	size_t getFaceCapacity() const;
	bool isReady() const;
};