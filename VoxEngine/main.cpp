#include "WindowManager.h"

#include "Graphics/Shader.h"
#include "Graphics/Camera.h"

#include <cstdint>
#include <iostream>
#include <memory>

#include "Chunk.h"

int main()
{
    try
    {
        WindowParams params{ 1280, 720, "My OpenGL 4.6 Window", true };
        WindowManager wnd(params);

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

        // World
		Chunk chunk;
        chunk.init(0, 0, 0);
        chunk.buildBlocks();
        chunk.buildMesh();

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

            chunk.render();
			glBindVertexArray(0); // Unbinding chunk's VAO for safety

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