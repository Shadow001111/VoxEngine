#include "WindowManager.h"

#include "Graphics/Shader.h"
#include "Graphics/Camera.h"

#include <cstdint>
#include <iostream>

struct Vertex
{
    float x, y;
};

struct Instance
{
    int32_t id;

	Instance(int32_t id) : id(id) {}
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
		std::vector<Instance> instances;
        for (size_t i = 0; i < 100; i++)
        {
            instances.emplace_back(i);
		}

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
        glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(Instance), instances.data() , GL_STATIC_DRAW);
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

        // Camera
        Camera camera({ 0.0f, 0.0f, 10.0f }, glm::radians(180.0f), 0.0f, glm::radians(90.0f), 1280.0f / 720.0f, 0.1f, 10.0f);

        // Face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Main loop
        while (!wnd.shouldClose())
        {
            // Rendering
            glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

			faceShader.use();
            {
                glm::mat4 view = camera.getViewMatrix();
                glm::mat4 projection = camera.getProjectionMatrix();

				faceShader.setMat4("view", view);
				faceShader.setMat4("projection", projection);
            }

            glBindVertexArray(vao);
            glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, instances.size());
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