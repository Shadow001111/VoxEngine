#include "VoxelMarkerMesh.h"

#include "OpenGLWrappers/OpenGLDebug.h"

VoxelMarkerMesh::VoxelMarkerMesh()
{
    const float vertices[] = {
        // +X face (right)
         0.5f, -0.5f,  0.5f,   1.0f, 0.0f, // 0
         0.5f, -0.5f, -0.5f,   0.0f, 0.0f, // 1
         0.5f,  0.5f, -0.5f,   0.0f, 1.0f, // 2
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, // 3

         // -X face (left)
         -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, // 4
         -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, // 5
         -0.5f,  0.5f,  0.5f,   0.0f, 1.0f, // 6
         -0.5f,  0.5f, -0.5f,   1.0f, 1.0f, // 7

         // +Y face (top)
         -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, // 8
          0.5f,  0.5f,  0.5f,   1.0f, 0.0f, // 9
          0.5f,  0.5f, -0.5f,   1.0f, 1.0f, //10
         -0.5f,  0.5f, -0.5f,   0.0f, 1.0f, //11

         // -Y face (bottom)
         -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, //12
          0.5f, -0.5f, -0.5f,   1.0f, 0.0f, //13
          0.5f, -0.5f,  0.5f,   1.0f, 1.0f, //14
         -0.5f, -0.5f,  0.5f,   0.0f, 1.0f, //15

         // +Z face (front)
         -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, //16
          0.5f, -0.5f,  0.5f,   1.0f, 0.0f, //17
          0.5f,  0.5f,  0.5f,   1.0f, 1.0f, //18
         -0.5f,  0.5f,  0.5f,   0.0f, 1.0f, //19

         // -Z face (back)
          0.5f, -0.5f, -0.5f,   0.0f, 0.0f, //20
         -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, //21
         -0.5f,  0.5f, -0.5f,   1.0f, 1.0f, //22
          0.5f,  0.5f, -0.5f,   0.0f, 1.0f  //23
    };


    const unsigned int indices[] = {
        // +X
        0, 1, 2,
        2, 3, 0,
        // -X
        4, 5, 6,
        6, 7, 4,
        // +Y
        8, 9,10,
        10,11, 8,
        // -Y
        12,13,14,
        14,15,12,
        // +Z
        16,17,18,
        18,19,16,
        // -Z
        20,21,22,
        22,23,20
    };

    indexCount = sizeof(indices) / sizeof(indices[0]);

    //
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

	// Bind VAO
	glBindVertexArray(vao);

	// VBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // vertex attributes
    // layout(location = 0) vec3 position;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    // layout(location = 1) vec2 uv;
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    //
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

VoxelMarkerMesh::~VoxelMarkerMesh()
{
	if (ebo)
	{
		glDeleteBuffers(1, &ebo);
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

void VoxelMarkerMesh::draw() const
{
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}
