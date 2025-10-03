#include "WindowManager.h"

#include "Graphics/Shader.h"
#include "Player.h"

#include <iostream>

#include "Chunk.h"

#include "UpdateTimer.h"

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

        // Player
		Player player({ 8.0f, 8.0f, 24.0f }, glm::radians(180.0f), 0.0f);
		player.getCamera().setAspectRatio(wnd.getAspectRatio());

        // Face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Depth test
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Time
		float lastTime = static_cast<float>(glfwGetTime());
		UpdateTimer playerUpdateTimer(20.0f);

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

			playerUpdateTimer.addTime(deltaTime);

			// Player
			if (playerUpdateTimer.shouldUpdate())
            {
				player.update(wnd, playerUpdateTimer.getUpdateInterval(), previousMousePos);
            }
			player.interpolateCameraTransform(playerUpdateTimer.getAccumulatedTimeInPercent());

            // Rendering
            glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			faceShader.use();
            {
				const Camera& camera = player.getCamera();

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