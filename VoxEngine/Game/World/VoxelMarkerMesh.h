#pragma once
#include <glad/glad.h>

class VoxelMarkerMesh
{
	GLuint vao, vbo, ebo;
	GLsizei indexCount;
public:
	VoxelMarkerMesh();
	~VoxelMarkerMesh();

	VoxelMarkerMesh(const VoxelMarkerMesh&) = delete;
	VoxelMarkerMesh& operator=(const VoxelMarkerMesh&) = delete;

	VoxelMarkerMesh(VoxelMarkerMesh&&) = delete;
	VoxelMarkerMesh& operator=(VoxelMarkerMesh&&) = delete;

	void draw() const;
};

