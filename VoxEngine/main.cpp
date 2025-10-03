#include "WindowManager.h"

#include "Graphics/Shader.h"
#include "Graphics/Camera.h"

#include <cstdint>
#include <iostream>
#include <memory>

struct Vertex
{
    float x, y;
};

struct Instance
{
    int32_t data;

    Instance(int x, int y, int z, int normal) : data(0)
    {
        // Coords 12 bits
		data |= (x & 15);
		data |= (y & 15) << 4;
		data |= (z & 15) << 8;

		// Normal 3 bits
		data |= (normal & 7) << 12;
    }
};

size_t getIndex(size_t x, size_t y, size_t z)
{
    return x + y * 16 + z * 16 * 16;
}

void buildChunk(std::vector<uint8_t>& blocks)
{
	blocks.resize(16 * 16 * 16, 0);

    for (size_t x = 0; x < 16; x++)
    {
        for (size_t y = 0; y < 16; y++)
        {
            for (size_t z = 0; z < 16; z++)
            {
                if (((x + y + z) & 1) == 0)
                {
                    blocks[getIndex(x, y, z)] = 1;
                }
            }
        }
	}
}

void buildChunkMesh(std::vector<Instance>& mesh, const std::vector<uint8_t>& blocks)
{
    mesh.clear();

    for (size_t x = 0; x < 16; x++)
    {
        for (size_t y = 0; y < 16; y++)
        {
            for (size_t z = 0; z < 16; z++)
            {
                if (blocks[getIndex(x, y, z)] == 0)
                    continue;
                // Check neighbors
                if (x == 0 || blocks[getIndex(x - 1, y, z)] == 0) // -X
                {
                    mesh.emplace_back(x, y, z, 0);
                }
                if (x == 15 || blocks[getIndex(x + 1, y, z)] == 0) // +X
                {
                    mesh.emplace_back(x, y, z, 1);
                }
                if (y == 0 || blocks[getIndex(x, y - 1, z)] == 0) // -Y
                {
                    mesh.emplace_back(x, y, z, 2);
                }
                if (y == 15 || blocks[getIndex(x, y + 1, z)] == 0) // +Y
                {
                    mesh.emplace_back(x, y, z, 3);
                }
                if (z == 0 || blocks[getIndex(x, y, z - 1)] == 0) // -Z
                {
                    mesh.emplace_back(x, y, z, 4);
                }
                if (z == 15 || blocks[getIndex(x, y, z + 1)] == 0) // +Z
                {
                    mesh.emplace_back(x, y, z, 5);
                }
            }
        }
	}
}

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
        std::vector<uint8_t> chunk;
		buildChunk(chunk);

		std::vector<Instance> instances;
		buildChunkMesh(instances, chunk);

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
        Camera camera({ 8.0f, 8.0f, 24.0f }, glm::radians(180.0f), 0.0f, glm::radians(90.0f), 1.0f, 0.1f, 100.0f);

        // Face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Depth test
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Time
		float lastTime = static_cast<float>(glfwGetTime());

        // Input
		float lastMouseX = 0.0f, lastMouseY = 0.0f;
		wnd.getMousePos(lastMouseX, lastMouseY);
        glfwSetInputMode(wnd.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Main loop
        while (!wnd.shouldClose())
        {
			// Time logic
			float time = static_cast<float>(glfwGetTime());
			float deltaTime = time - lastTime;
			lastTime = time;

			// Input
            {
                const float cameraSpeed = 15.0f * deltaTime;

				float leftRight = wnd.isKeyPressed(GLFW_KEY_D) - wnd.isKeyPressed(GLFW_KEY_A);
				float forwardBackward = wnd.isKeyPressed(GLFW_KEY_W) - wnd.isKeyPressed(GLFW_KEY_S);
				float worldUpDown = wnd.isKeyPressed(GLFW_KEY_SPACE) - wnd.isKeyPressed(GLFW_KEY_LEFT_CONTROL);

				camera.position += camera.right * leftRight * cameraSpeed;
				camera.position += camera.front * forwardBackward * cameraSpeed;
				camera.position += Camera::worldUp * worldUpDown * cameraSpeed;
            }
            {
                float mouseX, mouseY;
				wnd.getMousePos(mouseX, mouseY);

				const float mouseSensitivity = 0.002f;

                float offsetX = mouseX - lastMouseX;
				float offsetY = mouseY - lastMouseY;

				lastMouseX = mouseX;
				lastMouseY = mouseY;

				float yaw = camera.yaw;
				float pitch = camera.pitch;

				yaw -= offsetX * mouseSensitivity;
				pitch -= offsetY * mouseSensitivity;

				camera.setYawPitch(yaw, pitch);
            }

            // Rendering
            glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			faceShader.use();
            {
				camera.setAspectRatio(wnd.getAspectRatio());

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