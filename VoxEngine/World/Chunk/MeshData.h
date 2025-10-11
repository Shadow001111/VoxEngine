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
	uint16_t faceCount[6]; // Count for each side
private:
	uint16_t faceCapacity;
public:
	bool ready;

	std::vector<BlockFaceInstance> instances;

	ProcessingFence processingFence;

	MeshData();
	~MeshData();

	void resetFaceCount();
	void allocateMemoryForBuffer(size_t faceCount);
	void bindVAO() const;
	void bindInstanceVBO() const;

	size_t getFaceCountSum() const;
	size_t getFaceCapacity() const;
	bool isReady() const;
};