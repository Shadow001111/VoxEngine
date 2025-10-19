#include "WindowManager.h"

#include "Core/UpdateTimer.h"
#include "Core/Profiler.h"

#include "Game/World.h"
#include "Game/Player.h"

#include "Graphics/TextRenderer.h"

#include <iostream>
#include <sstream>
#include <iomanip>

std::string formatSize(size_t value)
{
    static const char* suffixes[] = { "", "k", "M", "G", "T", "P", "E" };
    constexpr size_t sufffixCount = sizeof(suffixes) / sizeof(suffixes[0]);
    double scaled = static_cast<double>(value);
    size_t suffixIndex = 0;

    while (scaled >= 1000.0 && suffixIndex < sufffixCount - 1)
    {
        scaled /= 1000.0;
        ++suffixIndex;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision((scaled < 10.0 && suffixIndex > 0) ? 1 : 0);
    oss << scaled << suffixes[suffixIndex];
    return oss.str();
}

std::string formatSizeBinary(size_t value)
{
    static const char* suffixes[] = { "", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB" };
    constexpr size_t sufffixCount = sizeof(suffixes) / sizeof(suffixes[0]);
    double scaled = static_cast<double>(value);
    size_t suffixIndex = 0;

    while (scaled >= 1024.0 && suffixIndex < sufffixCount - 1)
    {
        scaled /= 1024.0;
        ++suffixIndex;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision((scaled < 10.0 && suffixIndex > 0) ? 1 : 0);
    oss << scaled << suffixes[suffixIndex];
    return oss.str();
}

// TODO: Modern OpenGl
int main()
{
    constexpr int CHUNK_LOAD_DISTANCE = 12;

    constexpr float CAMERA_FAR_PLANE = (CHUNK_LOAD_DISTANCE + 0.5f) * (CHUNK_SIZE * 1.41f);
    constexpr float FOG_DISTANCE = (CHUNK_LOAD_DISTANCE + 0.5f)* CHUNK_SIZE;

    try
    {
        // Window
        WindowManager wnd({ 1280, 720, "My OpenGL 4.6 Window", true, true });

        // OpenGL states
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Text renderer
        TextRenderer::init();
        TextRenderer::loadFont("RusEngMinecraft", 8);
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
		UpdateTimer playerUpdateTimer(40.0f);
		UpdateTimer worldUpdateTimer(20.0f); worldUpdateTimer.setUpdateToTrue();
		UpdateTimer profilerUpdateTimer(1.0f / 3.0f);
        UpdateTimer frequentUIDataUpdateTimer(2.0f);

        // Frequent UI data
        float UI_FPS = 0.0f;
        float accumulatedFPS = 0.0f;
        int accumulatedFrames = 0;

        // Main loop
        while (!wnd.shouldClose())
        {
			// Poll events
            wnd.pollEvents();

			// Time logic
			float time = static_cast<float>(glfwGetTime());
			float deltaTime = time - lastTime;
			lastTime = time;

            const float FPS = 1.0f / deltaTime;
            accumulatedFPS += FPS;
            accumulatedFrames++;

			playerUpdateTimer.addTime(deltaTime);
			worldUpdateTimer.addTime(deltaTime);
			profilerUpdateTimer.addTime(deltaTime);
            frequentUIDataUpdateTimer.addTime(deltaTime);

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
				world.loadChunksAroundPlayer(playerPos, CHUNK_LOAD_DISTANCE);

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
            world.sortChunkMeshes(player.getCamera().getPosition());
            world.sendChunkMeshesToGPU();

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
            if (true)
            {
                // TODO: Average FPS
                const auto& debug = world.getDebugData();

                float rowHeight = 24.0f;
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(1);

                ss << "FPS: " << UI_FPS;

                if (wnd.getVSYNC())
                {
                    ss << " VSYNC";
                }

                // Chunks
                ss << "\nChunks: Loaded: " << formatSize(debug.loadedChunksCount)
                   << ", Rendered: " << formatSize(debug.renderedChunks);

                // Faces
                ss << "\nFaces: " << formatSize(debug.totalFaces)
                    << "/" << formatSize(debug.totalFaceCapacity)
                    << ", Rendered: " << formatSize(debug.renderedFaceCount);

                // Meshes
                ss << "\nChunk meshes: Capacity: " << formatSizeBinary(debug.totalFaceCapacity * sizeof(BlockFaceInstance))
                    << ", Gaps: " << formatSizeBinary(debug.chunkMeshesGaps * sizeof(BlockFaceInstance));

                // TODO: Add textures and font size in bytes

                // Player orientation
                const Camera& camera = player.getCamera();
                const auto& cameraPos = camera.getPosition();
                const auto& cameraViewDirection = camera.getForward();

                ss << "\nX: " << cameraPos.x << " Y: " << cameraPos.y << " Z: " << cameraPos.z;
                
                std::string facingDir;
                {
                    float absX = std::abs(cameraViewDirection.x);
                    float absY = std::abs(cameraViewDirection.y);
                    float absZ = std::abs(cameraViewDirection.z);
                    if (absX > absY && absX > absZ)
                    {
                        facingDir = (cameraViewDirection.x > 0.0f) ? "+X" : "-X";
                    }
                    else if (absY > absX && absY > absZ)
                    {
                        facingDir = (cameraViewDirection.y > 0.0f) ? "+Y" : "-Y";
                    }
                    else
                    {
                        facingDir = (cameraViewDirection.z > 0.0f) ? "+Z" : "-Z";
                    }
                }
                ss << "\nView direction: " << facingDir;

                std::string text = ss.str();
                TextRenderer::renderText(text, 10.0f, wnd.getHeight() - 10.0f - rowHeight, rowHeight, glm::vec3(1.0f, 0.0f, 0.0f));
            }

            // Swap buffers
            wnd.swapBuffers();

            //
            if (frequentUIDataUpdateTimer.shouldUpdate())
            {
                UI_FPS = accumulatedFPS / accumulatedFrames;
                accumulatedFPS = 0.0f;
                accumulatedFrames = 0;
            }

            if (profilerUpdateTimer.shouldUpdate())
            {
                //Profiler::printProfileReport();
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