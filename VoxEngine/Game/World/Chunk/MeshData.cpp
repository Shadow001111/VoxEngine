#include "MeshData.h"

#include <glm/vec2.hpp>

MeshData::MeshData() :
	vao(0), vbo(0), instanceVBO(0),
	faceCapacity(0),
	ready(false), dirty(false)
{
	resetFaceCount();

	// Create buffers once
	glm::vec2 vertices[4] = // CCW order
	{
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f }
	};

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &instanceVBO);

	// Bind VAO
	glBindVertexArray(vao);

	// Vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	// Instance buffer
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 2, GL_INT, sizeof(BlockFaceInstance), (void*)0); // integer attribute
	glVertexAttribDivisor(1, 1); // advance per instance
}

MeshData::~MeshData()
{
	if (instanceVBO)
	{
		glDeleteBuffers(1, &instanceVBO);
	}
	if (vbo)
	{
		glDeleteBuffers(1, &vbo);
	}
	if (vao)
	{
		glDeleteVertexArrays(1, &vao);
	}
}

void MeshData::resetFaceCount()
{
	for (int i = 0; i < 6; i++)
	{
		opaqueFaceCount[i] = 0;
		transparentFaceCount[i] = 0;
	}
}

void MeshData::allocateMemoryForBuffer(size_t faceCount)
{
	if (faceCount > faceCapacity)
	{
		faceCapacity = faceCount;
		glBufferData(GL_ARRAY_BUFFER, faceCapacity * sizeof(BlockFaceInstance), nullptr, GL_STATIC_DRAW);
	}
}

void MeshData::bindVAO() const
{
	glBindVertexArray(vao);
}

void MeshData::bindInstanceVBO() const
{
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
}

size_t MeshData::getOpaqueFaceCountSum() const
{
	size_t sum = 0;
	for (int i = 0; i < 6; i++)
	{
		sum += opaqueFaceCount[i];
	}
	return sum;
}

size_t MeshData::getTransparentFaceCountSum() const
{
	size_t sum = 0;
	for (int i = 0; i < 6; i++)
	{
		sum += transparentFaceCount[i];
	}
	return sum;
}

size_t MeshData::getFaceCapacity() const
{
	return faceCapacity;
}