#include "WindowManager.h"

#include "Graphics/Shader.h"
#include "Player.h"

#include <iostream>
#include <memory>

#include "Chunk.h"

#include "UpdateTimer.h"


void loadChunks(std::unordered_map<Int3, std::unique_ptr<Chunk>, ChunkHash>& chunks, int playerChunkX, int playerChunkY, int playerChunkZ)
{
    const int renderDistance = 2; // In chunks
    for (int x = -renderDistance; x <= renderDistance; x++)
    {
        for (int y = -renderDistance; y <= renderDistance; y++)
        {
            for (int z = -renderDistance; z <= renderDistance; z++)
            {
                int chunkX = playerChunkX + x;
                int chunkY = playerChunkY + y;
                int chunkZ = playerChunkZ + z;

                auto chunk = std::make_unique<Chunk>();
                chunk->init(chunkX, chunkY, chunkZ);
                chunk->buildBlocks();
                chunk->buildMesh();

				chunks[chunk->getPosition()] = std::move(chunk);
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

        // Input
        glm::vec2 previousMousePos;
		wnd.getMousePos(previousMousePos.x, previousMousePos.y);
        glfwSetInputMode(wnd.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // World
		std::unordered_map<Int3, std::unique_ptr<Chunk>, ChunkHash> chunks;
		int playerChunkX = static_cast<int>(floor(player.getPosition().x)) / CHUNK_SIZE;
		int playerChunkY = static_cast<int>(floor(player.getPosition().y)) / CHUNK_SIZE;
		int playerChunkZ = static_cast<int>(floor(player.getPosition().z)) / CHUNK_SIZE;
		loadChunks(chunks, playerChunkX, playerChunkY, playerChunkZ);

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

            for (const auto& pair : chunks)
            {
                const Chunk* chunk = pair.second.get();

				Int3 pos = chunk->getPosition();
				glm::vec3 chunkWorldPos = glm::vec3(pos.x, pos.y, pos.z) * static_cast<float>(CHUNK_SIZE);

				faceShader.setVec3("chunkPosition", chunkWorldPos.x, chunkWorldPos.y, chunkWorldPos.z);

                chunk->render();
            }
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