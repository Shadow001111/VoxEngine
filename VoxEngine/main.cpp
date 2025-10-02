#include "WindowManager.h"
#include "Graphics/Shader.h"

#include <cstdint>
#include <iostream>

struct Vertex
{
    float x, y;
};

struct Instance
{
    int32_t id;
};

int main()
{
    try
    {
        WindowParams params{ 1280, 720, "My OpenGL 4.6 Window", true };
        WindowManager wnd(params);

        // Vertices
		Vertex vertices[4] = // CCW order
        {
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 1.0f, 1.0f },
			{ 0.0f, 1.0f }
        };

        // Instance data
        Instance instances[3] = { {0}, {1}, {2} };

        // Buffers
        GLuint vao, vbo, instanceVBO;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &instanceVBO);

        glBindVertexArray(vao);

        // Vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

        // Instance buffer
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(instances), instances, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribIPointer(1, 1, GL_INT, sizeof(Instance), (void*)0); // integer attribute
        glVertexAttribDivisor(1, 1); // advance per instance

        glBindVertexArray(0);

        // Shaders sources
        std::vector<Shader::ShaderSource> faceShaderSources =
        {
            {GL_VERTEX_SHADER, "Shaders/face.vert"},
            {GL_FRAGMENT_SHADER, "Shaders/face.frag"}
        };

        Shader faceShader(faceShaderSources);
        faceShaderSources.clear();

        // Main loop
        while (!wnd.shouldClose())
        {
            // Rendering
            glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

			faceShader.use();

            glBindVertexArray(vao);
            glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, 3);
            glBindVertexArray(0);

            wnd.swapBuffers();
            wnd.pollEvents();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
	return 0;
}