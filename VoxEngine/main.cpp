#include "WindowManager.h"

#include "Graphics/Shader.h"
#include "Graphics/Camera.h"

#include <iostream>
#include <memory>

#include "Chunk.h"

#include "UpdateTimer.h"

void playerInput(const WindowManager& wnd, Camera& camera, float deltaTime, glm::vec2& lastMousePos)
{
    const float cameraSpeed = 15.0f * deltaTime;
    const float mouseSensitivity = 0.002f;
    // Position
    {
        float leftRight = wnd.isKeyPressed(GLFW_KEY_D) - wnd.isKeyPressed(GLFW_KEY_A);
        float forwardBackward = wnd.isKeyPressed(GLFW_KEY_W) - wnd.isKeyPressed(GLFW_KEY_S);
        float worldUpDown = wnd.isKeyPressed(GLFW_KEY_SPACE) - wnd.isKeyPressed(GLFW_KEY_LEFT_CONTROL);

		glm::vec3 movementVector = glm::vec3(0.0f);

        movementVector += camera.getRight() * leftRight;
        movementVector += camera.getFront() * forwardBackward;
        movementVector.y += worldUpDown;

        if (glm::length(movementVector) > 0.0f)
        {
            movementVector = glm::normalize(movementVector) * cameraSpeed;
            camera.move(movementVector);
        }
    }
    // Rotation
    {
        float mouseX, mouseY;
        wnd.getMousePos(mouseX, mouseY);

        float offsetX = mouseX - lastMousePos.x;
        float offsetY = mouseY - lastMousePos.y;

        lastMousePos.x = mouseX;
        lastMousePos.y = mouseY;

        camera.rotate(-offsetX * mouseSensitivity, -offsetY * mouseSensitivity);
    }
}

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
        Transform currentCameraTransform = camera.getTransform();
        Transform previousCameraTransform = currentCameraTransform;

        // Face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Depth test
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Time
		float lastTime = static_cast<float>(glfwGetTime());
		UpdateTimer playerInputTimer(20.0f);

        // Input
        glm::vec2 previousMousePos;
		wnd.getMousePos(previousMousePos.x, previousMousePos.y);
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

			playerInputTimer.addTime(deltaTime);

			// Input
			if (playerInputTimer.shouldUpdate())
            {
				previousCameraTransform = currentCameraTransform;
				playerInput(wnd, camera, playerInputTimer.getUpdateInterval(), previousMousePos);
				currentCameraTransform = camera.getTransform();
            }

            // Rendering
            glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			faceShader.use();
            {
                Transform interpolatedCameraTransform = previousCameraTransform.interpolate(currentCameraTransform, playerInputTimer.getAccumulatedTimeInPercent());
                camera.setTransform(interpolatedCameraTransform);
				camera.setAspectRatio(wnd.getAspectRatio());

                glm::mat4 view = camera.getViewMatrix();
                glm::mat4 projection = camera.getProjectionMatrix();

				camera.setTransform(currentCameraTransform); // Restore current transform

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