#include "WindowManager.h"

#include "Core/UpdateTimer.h"
#include "Core/Profiler.h"

#include "Game/World.h"
#include "Game/Player.h"

#include "Graphics/TextRenderer.h"

#include <iostream>
#include <sstream>
#include <iomanip>

// TODO: Modern OpenGl
int main()
{
    constexpr int CHUNK_LOAD_DISTANCE = 4;

    constexpr float CAMERA_FAR_PLANE = (CHUNK_LOAD_DISTANCE + 0.5f) * (CHUNK_SIZE * 1.41f);
    constexpr float FOG_DISTANCE = (CHUNK_LOAD_DISTANCE + 0.5f)* CHUNK_SIZE;

    try
    {
        // Window
        WindowManager wnd({ 1280, 720, "My OpenGL 4.6 Window", true });

        // OpenGL states
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Text renderer
        TextRenderer::init();
        TextRenderer::loadFont("RusEngMinecraft", 64);
        TextRenderer::setCurrentFont("RusEngMinecraft");

        // Player
        Player player({ 0.0f, 20.0f, 0.0f }, glm::radians(180.0f), 0.0f);
        player.getCamera().setAspectRatio(wnd.getAspectRatio());
        player.getCamera().setFarPlane(CAMERA_FAR_PLANE);

        // World
        World world;
        world.preparation(CHUNK_LOAD_DISTANCE);

        world.visuals.backgroundColor = { 0.52f, 0.8f, 0.92f }; // Sky color
        world.visuals.fogGradient = 5.0f;
        world.visuals.fogDensity = world.visuals.calculateFogDensity(FOG_DISTANCE, world.visuals.fogGradient);

        // Input
        glm::vec2 previousMousePos;
        wnd.getMousePos(previousMousePos.x, previousMousePos.y);
        glfwSetInputMode(wnd.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Timers
		float lastTime = static_cast<float>(glfwGetTime());
		UpdateTimer playerUpdateTimer(20.0f);
		UpdateTimer worldUpdateTimer(20.0f); worldUpdateTimer.setUpdateToTrue();
		UpdateTimer profilerUpdateTimer(1.0f / 3.0f);
        //UpdateTimer debugUpdateTimer(10.0f);

        // Main loop
        while (!wnd.shouldClose())
        {
			// Poll events
            wnd.pollEvents();

			// Time logic
			float time = static_cast<float>(glfwGetTime());
			float deltaTime = time - lastTime;
			lastTime = time;

			playerUpdateTimer.addTime(deltaTime);
			worldUpdateTimer.addTime(deltaTime);
			profilerUpdateTimer.addTime(deltaTime);
            //debugUpdateTimer.addTime(deltaTime);

            // Player
            PlayerInput playerInput;
            if (playerUpdateTimer.peek())
            {
                playerInput.moveForward = wnd.isKeyPressed(GLFW_KEY_W);
                playerInput.moveBackward = wnd.isKeyPressed(GLFW_KEY_S);
                playerInput.moveLeft = wnd.isKeyPressed(GLFW_KEY_A);
                playerInput.moveRight = wnd.isKeyPressed(GLFW_KEY_D);
                playerInput.moveUp = wnd.isKeyPressed(GLFW_KEY_SPACE);
                playerInput.moveDown = wnd.isKeyPressed(GLFW_KEY_LEFT_CONTROL);
                playerInput.sprint = wnd.isKeyPressed(GLFW_KEY_LEFT_SHIFT);

                float mouseX, mouseY;
                wnd.getMousePos(mouseX, mouseY);
                playerInput.mouseDelta = glm::vec2(mouseX - previousMousePos.x, mouseY - previousMousePos.y);
                previousMousePos = glm::vec2(mouseX, mouseY);
            }
            
            while (playerUpdateTimer.shouldUpdate())
            {
                player.update(playerInput, playerUpdateTimer.getUpdateInterval());
                playerInput.mouseDelta = glm::vec2(0.0f, 0.0f);
            }
            player.interpolateCameraTransform(playerUpdateTimer.getAccumulatedTimeInPercent());
            
            World::RaycastResult playerRaycastResult;
            {
                const Camera& camera = player.getCamera();
                const Transform& transform = camera.getTransform();
                playerRaycastResult = world.raycast(transform.position, camera.getForward(), 16.0f);
            }

            // World
            while (worldUpdateTimer.shouldUpdate())
            {
				glm::vec3 playerPos = player.getPosition();
                Int3 playerChunkPos(
                    static_cast<int>(floorf(playerPos.x / CHUNK_SIZE)),
                    static_cast<int>(floorf(playerPos.y / CHUNK_SIZE)),
                    static_cast<int>(floorf(playerPos.z / CHUNK_SIZE))
                );

				world.loadChunksAroundPlayer(playerChunkPos, CHUNK_LOAD_DISTANCE);
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

            // Rendering
            {
                const auto& color = world.visuals.backgroundColor;
                glClearColor(color.r, color.g, color.b, 1.0f);
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            TextRenderer::updateProjectionMatrix(wnd.getWidth(), wnd.getHeight());

			world.renderChunks(player.getCamera());
            world.renderVoxelMarker(player.getCamera(), playerRaycastResult);

            // Text rendering
            {
                const auto& debug = world.getDebugData();

                float rowHeight = 32.0f;
                std::ostringstream ss;

                // TODO: Display VSYNC and MAX-FPS
                ss << "FPS: " << std::to_string(int(1.0f / deltaTime)) << "\n";

                // Chunks
                ss << "Chunks: Loaded: " << std::to_string(debug.loadedChunksCount)
                   << ",  Rendered: " <<     std::to_string(debug.renderedChunks);

                // Player orientation
                const auto& playerPos = player.getCamera().getPosition();
                ss << std::fixed << std::setprecision(1);
                ss << "X:" << playerPos.x << "  Y:" << playerPos.y << "  Z:" << playerPos.z;

                std::string text = ss.str();
                TextRenderer::renderText(text, 10.0f, wnd.getHeight() - 40.0f, rowHeight, glm::vec3(1.0f, 0.0f, 0.0f));
            }

            // Swap buffers
            wnd.swapBuffers();

            if (profilerUpdateTimer.shouldUpdate())
            {
                Profiler::printProfileReport();
            }
        }

        //
        Profiler::printProfileReport();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
	return 0;
}