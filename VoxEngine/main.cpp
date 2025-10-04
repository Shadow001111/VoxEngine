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
        // Window
        WindowManager wnd({ 1280, 720, "My OpenGL 4.6 Window", true });

        // Shaders
        std::vector<Shader::ShaderSource> faceShaderSources =
        {
            {GL_VERTEX_SHADER, "Shaders/face.vert"},
            {GL_FRAGMENT_SHADER, "Shaders/face.frag"}
        };

        Shader faceShader(faceShaderSources);
        faceShaderSources.clear();

        // OpenGL states
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Player
        Player player({ 0.0f, 2.0f, 0.0f }, glm::radians(180.0f), 0.0f);
        player.getCamera().setAspectRatio(wnd.getAspectRatio());

        // World
        World world;

        // Input
        glm::vec2 previousMousePos;
        wnd.getMousePos(previousMousePos.x, previousMousePos.y);
        glfwSetInputMode(wnd.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Timers
		float lastTime = static_cast<float>(glfwGetTime());
		UpdateTimer playerUpdateTimer(20.0f);
		UpdateTimer worldUpdateTimer(20.0f); worldUpdateTimer.setUpdateToTrue();
		UpdateTimer profilerUpdateTimer(1.0f / 3.0f);
        UpdateTimer debugUpdateTimer(10.0f);

        // Main loop
        while (!wnd.shouldClose())
        {
			Profiler::beginFrame();

            // Poll events
            wnd.pollEvents();

			// Time logic
			float time = static_cast<float>(glfwGetTime());
			float deltaTime = time - lastTime;
			lastTime = time;

			playerUpdateTimer.addTime(deltaTime);
			worldUpdateTimer.addTime(deltaTime);
			profilerUpdateTimer.addTime(deltaTime);
            debugUpdateTimer.addTime(deltaTime);

            // World
            if (worldUpdateTimer.shouldUpdate())
            {
				glm::vec3 playerPos = player.getPosition();
                Int3 playerChunkPos(
                    static_cast<int>(floorf(playerPos.x / CHUNK_SIZE)),
                    static_cast<int>(floorf(playerPos.y / CHUNK_SIZE)),
                    static_cast<int>(floorf(playerPos.z / CHUNK_SIZE))
                );

				world.loadChunksAroundPlayer(playerChunkPos, 8);
				world.update();

                if (wnd.isKeyPressed(GLFW_KEY_P))
                {
                    world.rebuildAllChunkMeshes();
                    std::cout << "World: All chunks meshes are rebuild." << std::endl;
                }

                if (wnd.isKeyPressed(GLFW_KEY_O))
                {
                    world.debugMethod();
                }
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
				faceShader.setMat4("view", camera.getViewMatrix());
				faceShader.setMat4("projection", camera.getProjectionMatrix());
            }

			world.render(faceShader);

            // Swap buffers
            wnd.swapBuffers();

            //Profiler
            Profiler::endFrame();

            // Debug
            if (debugUpdateTimer.shouldUpdate())
            {
                size_t totalFaces, totalFaceCapacity, potentialMaximumCapacity;
                world.getChunkMeshesInfo(totalFaces, totalFaceCapacity, potentialMaximumCapacity);

                std::string title = "Faces/Capacity/Maximum: "
                    + std::to_string(totalFaces >> 10) + "k/"
                    + std::to_string(totalFaceCapacity >> 10) + "k/"
                    + std::to_string(potentialMaximumCapacity >> 10) + "k";

                wnd.setTitle(title);
            }

            if (profilerUpdateTimer.shouldUpdate())
            {
                Profiler::printProfileReport();
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