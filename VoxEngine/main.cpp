#include "WindowManager.h"

#include "Graphics/Shader.h"

#include <iostream>

#include "World.h"
#include "Player.h"

#include "UpdateTimer.h"
#include "Profiler.h"

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
		Player player({ 0.0f, 2.0f, 0.0f }, glm::radians(180.0f), 0.0f);
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
		UpdateTimer worldUpdateTimer(20.0f); worldUpdateTimer.setUpdateToTrue();
		UpdateTimer profilerUpdateTimer(1.0f / 3.0f);

        // Input
        glm::vec2 previousMousePos;
		wnd.getMousePos(previousMousePos.x, previousMousePos.y);
        glfwSetInputMode(wnd.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // World
        World world;

        // Main loop
        while (!wnd.shouldClose())
        {
			Profiler::beginFrame();

			// Time logic
			float time = static_cast<float>(glfwGetTime());
			float deltaTime = time - lastTime;
			lastTime = time;

			playerUpdateTimer.addTime(deltaTime);
			worldUpdateTimer.addTime(deltaTime);
			profilerUpdateTimer.addTime(deltaTime);

            // World
            if (worldUpdateTimer.shouldUpdate())
            {
				glm::vec3 playerPos = player.getPosition();
				int playerChunkX = int(floorf(playerPos.x / (float)CHUNK_SIZE));
                int playerChunkY = int(floorf(playerPos.y / (float)CHUNK_SIZE));
                int playerChunkZ = int(floorf(playerPos.z / (float)CHUNK_SIZE));
				Int3 playerChunkPos(playerChunkX, playerChunkY, playerChunkZ);
				world.loadChunksAroundPlayer(playerChunkPos, 2);
				world.update();
            }

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

			world.render(faceShader);

            wnd.swapBuffers();
            wnd.pollEvents();

            //Profiler
            Profiler::endFrame();

            if (profilerUpdateTimer.shouldUpdate())
            {
                //Profiler::printProfileReport();
				Profiler::resetAllProfiles();
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
	return 0;
}