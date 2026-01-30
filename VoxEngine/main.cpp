#include "WindowManager.h"

#include "Core/UpdateTimer.h"
#include "Core/Profiler.h"

#include "Game/World.h"
#include "Game/Player/Player.h"
#include "Game/DataPackManagment/AssetRegistry.h"

#include "Graphics/TextRenderer.h"
#include "Graphics/quad_vertices.h"
#include "Graphics/TextureLoader.h"

#include "OpenGLWrappers/VertexArray.h"
#include "OpenGLWrappers/ImmutableBuffer.h"

#include "SoundManager.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <glm/gtc/matrix_transform.hpp>

#ifdef NDEBUG
constexpr int CHUNK_LOAD_DISTANCE = 1;
#else
constexpr int CHUNK_LOAD_DISTANCE = 3;
#endif


static std::string formatSize(size_t value)
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

static std::string formatSizeBinary(size_t value)
{
    static const char* suffixes[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB" };
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

static void setupOpenGLStates()
{
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

struct ContainerUI
{
    Shader hotbarShader;
    Texture hotbarSlotImage;
    VertexArray hotbarVAO;
    ImmutableBuffer hotbarVBO;

    Texture itemUITextureArray;
};

struct DebugUIMetrics
{
    double fps = 0.0;
    double frameTimeMs = 0.0;  // Accumulated milliseconds per frame
    size_t loadedChunksCount = 0;
    size_t renderedChunks = 0;
    size_t totalFaces = 0;
    size_t totalFaceCapacityInBytes = 0;
    size_t renderedFaceCount = 0;
    size_t chunkDrawCommandBufferSizeInBytes = 0;
    size_t chunkPositionBufferSizeInBytes = 0;
};

static void setupContainerUI(ContainerUI& c)
{
    {
        std::vector<Shader::ShaderSource> sources =
        {
            {GL_VERTEX_SHADER, "res/Shaders/hotbar.vert"},
            {GL_FRAGMENT_SHADER, "res/Shaders/hotbar.frag"}
        };
        c.hotbarShader.create(sources);
        c.hotbarShader.setInt("uTexture", 0);
    }

    {
        c.hotbarVAO.create();

        c.hotbarVBO.create(GL_ARRAY_BUFFER);
        c.hotbarVBO.allocateStorage(sizeof(quadVertices), 0, quadVertices);

        c.hotbarVAO.bindVertexBuffer(0, c.hotbarVBO.getID(), 0, 4 * sizeof(float));

        c.hotbarVAO.enableAttribute(0);
        c.hotbarVAO.setFloatAttribute(0, 4, 0);
    }

    {
        std::vector<std::string> itemTextureNames = AssetRegistry::getItemUITextureNames();

        TextureLoader::TextureParams params;

        PROFILE_SCOPE("Item ui texture array creation", ProfileCategory::General);
        TextureLoader::createTextureArrayFromImages(c.itemUITextureArray, "res/ItemUITextures", itemTextureNames, params);

        c.itemUITextureArray.setParameters(GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    {
        TextureLoader::TextureParams params;

        TextureLoader::createTextureArrayFromImages(c.hotbarSlotImage, "res/UITextures", { "empty_hotbar_slot", "selected_hotbar_slot" }, params);

        c.hotbarSlotImage.setParameters(GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }
}

static void renderInventory(const float aspectRatio, const GUIInventory& inventory, const ContainerUI& c)
{
    // Settings
    constexpr float TEXTURE_PX = 24.0f;
    constexpr float EMPTY_PX = 16.0f;
    constexpr float SELECTED_PX = 22.0f;
    constexpr float ITEM_PX = EMPTY_PX - 4.0f;

    // Get invetory dimensions
    const auto columnCount = inventory.getColumnCount();
    const auto rowCount = inventory.getRowCount();
    const auto slotCount = inventory.getSlotCount();

    // Calculate slot size
    const auto& grid = inventory.getVisualGrid();

    const float gridWidth = grid.getWidth();
    const float slotSize = gridWidth / columnCount;

    // Calculate image scales
    const float emptyScale = (TEXTURE_PX / EMPTY_PX) * slotSize;
    const float selectedScale = emptyScale;
    const float itemScale = (ITEM_PX / EMPTY_PX) * slotSize;

    // Get projection
    glm::mat4 projection = glm::ortho(
        -aspectRatio, aspectRatio,
        -1.0f, 1.0f,
        -1.0f, 1.0f
    );

    // Calculate grid start coordinates
    const float startX = grid.leftBorder + slotSize * 0.5f;
    const float slotDeltaY = grid.gridGoesUp ? slotSize : -slotSize;
    const float startY = grid.borderY + slotDeltaY * 0.5f;

    // Bind resources
    c.hotbarShader.use();
    c.hotbarVAO.bind();

    // Render empty slots
    // TODO: Use tiling instead of drawing many identical quads
    c.hotbarShader.setUint("uTextureId", 0);
    c.hotbarShader.setMat4("projection", projection);

    c.hotbarSlotImage.bindUnit(0);
    for (uint32_t row = 0; row < rowCount; row++)
    {
        for (uint32_t col = 0; col < columnCount; col++)
        {
            float x = startX + col * slotSize;
            float y = startY + row * slotDeltaY;

            glm::mat4 model(1.0f);
            model = glm::translate(model, { x, y, 0.0f });
            model = glm::scale(model, { emptyScale * 0.5f, emptyScale * 0.5f, 1.0f });

            c.hotbarShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }

    // Collect items that need count text
    struct ItemCountInfo
    {
        float x, y;
        uint16_t count;
    };
    std::vector<ItemCountInfo> itemsWithCount;

    // Render items and collect those that need count text
    c.itemUITextureArray.bindUnit(0);
    size_t index = -1;
    for (uint32_t row = 0; row < rowCount; row++)
    {
        for (uint32_t col = 0; col < columnCount; col++)
        {
            index++;
            if (index > slotCount)
            {
                break;
            }
    
            // Get item
            const Item& item = *inventory.getItemAt(index);
            if (item.count == 0)
            {
                continue;
            }
    
            // Get item data
            const auto* itemData = AssetRegistry::getItemData(item.id);
            if (!itemData)
            {
                continue;
            }
    
            float x = startX + col * slotSize;
            float y = startY + row * slotDeltaY;
    
            glm::mat4 model(1.0f);
            model = glm::translate(model, { x, y, 0.0f });
            model = glm::scale(model, { itemScale * 0.5f, itemScale * 0.5f, 1.0f });
    
            c.hotbarShader.setMat4("model", model);
            c.hotbarShader.setUint("uTextureId", itemData->uiTextureId);
    
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            // Collect item for count text if needed
            if (item.count > 1)
            {
                itemsWithCount.emplace_back(x, y, item.count);
            }
        }
    }

    // Render item counts after all items are drawn (single shader switch)
    const float COUNT_FONT_SIZE = slotSize * 0.25f;
    if (!itemsWithCount.empty())
    {
        // Prepare text rendering
        TextRenderer::startTextRendering();

        // Render counts for all items
        for (const auto& itemInfo : itemsWithCount)
        {
            // Text position
            const float cornerOffset = slotSize * 0.1f;
            const float shadowOffset = slotSize * 0.02f;

            const float x = itemInfo.x + slotSize * 0.5f - cornerOffset;
            const float y = itemInfo.y - slotSize * 0.5f + cornerOffset;

            // Format count text
            std::string countText = (itemInfo.count > 999) ? "999+" : std::to_string(itemInfo.count);

            // Render count with appropriate color
            glm::vec3 textColor = glm::vec3(1.0f, 1.0f, 1.0f);

            // Add drop shadow for better readability
            TextRenderer::renderText(countText, x + shadowOffset, y - shadowOffset, COUNT_FONT_SIZE, glm::vec3(0.0f, 0.0f, 0.0f), TextRenderer::TextAlignment::BottomRight);
            TextRenderer::renderText(countText, x, y, COUNT_FONT_SIZE, textColor, TextRenderer::TextAlignment::BottomRight);
        }
    }
}

// TODO: Stop hotbar from drawing itself on other draw buffers
static void renderHotbarAndInventory(const float aspectRatio, const ContainerUI& c, const Player& player)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    renderInventory(aspectRatio, player.getHotbar(), c); // TODO: Add rendering choosen slot
    if (player.isInventoryOpened())
    {
        renderInventory(aspectRatio, player.getInventory(), c);
    }

    glDepthMask(GL_TRUE);
}

static void renderUI(const float aspectRatio, const ContainerUI& c, const Player& player)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderHotbarAndInventory(aspectRatio, c, player);
}

static void renderDebugData(const WindowManager& wnd, const Player& player, const DebugUIMetrics& metrics)
{
    const float rowHeight = 0.06f;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);

    ss << "FPS: " << metrics.fps;
    ss << " (" << metrics.frameTimeMs << " ms)";

    if (wnd.getVSYNC())
    {
        ss << " VSYNC";
    }

    // Chunks
    ss << "\nChunks: Loaded: " << formatSize(metrics.loadedChunksCount)
        << ", Rendered: " << formatSize(metrics.renderedChunks);

    // Faces
    ss << "\nFaces: " << formatSize(metrics.totalFaces)
        << "/" << formatSize(metrics.totalFaceCapacityInBytes)
        << ", Rendered: " << formatSize(metrics.renderedFaceCount);

    // Meshes
    ss << "\nChunk meshes: Capacity: " << formatSizeBinary(metrics.totalFaceCapacityInBytes);

    // Buffer sizes
    ss << "\nChunk draw command buffer: " << formatSizeBinary(metrics.chunkDrawCommandBufferSizeInBytes);
    ss << "\nChunk position buffer: " << formatSizeBinary(metrics.chunkPositionBufferSizeInBytes);

    // TODO: Add textures and font size in bytes

    // Player orientation
    const Camera& camera = player.getCamera();
    const auto& playerPos = player.getPosition();
    const auto& cameraViewDirection = camera.getForward();

    glm::ivec3 localPlayerPos = glm::ivec3(glm::mod(glm::mod(playerPos, (double)CHUNK_SIZE) + (double)CHUNK_SIZE, (double)CHUNK_SIZE));

    ss << "\nXYZ: " << playerPos.x << " " << playerPos.y << " " << playerPos.z;
    ss << "\nBlock: " << localPlayerPos.x << " " << localPlayerPos.y << " " << localPlayerPos.z;

    std::string facingDir;
    {
        float absX = std::abs(cameraViewDirection.x);
        float absY = std::abs(cameraViewDirection.y);
        float absZ = std::abs(cameraViewDirection.z);
        if (absX > absY && absX > absZ) {
            facingDir = (cameraViewDirection.x > 0.0f) ? "+X" : "-X";
        }
        else if (absY > absX && absY > absZ) {
            facingDir = (cameraViewDirection.y > 0.0f) ? "+Y" : "-Y";
        }
        else {
            facingDir = (cameraViewDirection.z > 0.0f) ? "+Z" : "-Z";
        }
    }
    ss << "\nView direction: " << facingDir;

    // Render text

    std::string text = ss.str();

    TextRenderer::startTextRendering();
    TextRenderer::renderText(text, -wnd.getAspectRatio() + 0.014f, 1.0f - 0.014f, rowHeight, glm::vec3(1.0f, 0.0f, 0.0f));

    glDepthMask(GL_TRUE);
}

void check()
{
    // Data
    GLint maxSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);

    std::cout << std::string(100, '=') << "\n";
    std::cout << "Data:\n";
    std::cout << "    OpenGL Version: " << glGetString(GL_VERSION)  << "\n";
    std::cout << "    GPU Vendor: " << glGetString(GL_VENDOR)       << "\n";
    std::cout << "    Renderer: " << glGetString(GL_RENDERER)       << "\n";

    //std::cout << "Secondary:\n";
    //std::cout << "    Max texture size: " << maxSize << "\n";

    // Check texture compression available
    //std::cout << std::string(100, '=') << "\n";
    //{
    //    checkAllCompressionFormats();
    //}

    //
    std::cout << std::string(100, '=') << "\n";
}


// TODO: Fix terrain generation at far lands
// TODO: Fix: GUI disappears when window gets resized
int main()
{
    constexpr float CAMERA_FAR_PLANE = (CHUNK_LOAD_DISTANCE + 0.5f) * CHUNK_SIZE;

    //
    std::ios_base::sync_with_stdio(false);

    // Window
    WindowManager wnd({ 1280, 720, "VoxEngine", true, true, true });

    // Check
    check();

    // Create framebuffer
    FrameBuffer framebuffer;
    {
        framebuffer.create(wnd.getWidth(), wnd.getHeight());

        framebuffer.createColorAttachment("color", GL_RGBA8);
        framebuffer.createColorAttachment("geometryAlpha", GL_R8);
        framebuffer.createColorAttachment("accumulation", GL_RGBA16F);
        framebuffer.createColorAttachment("revealage", GL_R8);

        // TODO: Render aurora in lower resolution
        // Problem: Geometry alpha mask is in higher resolution, meaning we need to sample multiple pixels to check if geometry covers aurora or not.
        framebuffer.createStandaloneTextureAttachment("aurora", GL_RGBA8);

        framebuffer.createDepthAttachment("depth", GL_DEPTH_COMPONENT32F);

        framebuffer.setDrawBuffers({ "color", "geometryAlpha", "accumulation", "revealage" });

        if (!framebuffer.isComplete())
        {
            std::cerr << "[main]: Failed to create framebuffer\n";
            return -1;
        }

        wnd.linkFramebuffer(&framebuffer);

        framebuffer.bind();
    }

    // OpenGL states
    setupOpenGLStates();

    // Text renderer
    TextRenderer::init();
    TextRenderer::loadFont("RusEngMinecraft", 8);
    TextRenderer::setCurrentFont("RusEngMinecraft");
    TextRenderer::setGlyphInstanceBatchSize(1024);

    // World
    World world;
    world.setChunkLoadingDistance(CHUNK_LOAD_DISTANCE);
    world.preparation();

    // Player
    Player& player = *world.createEntity<Player>(glm::vec3(0.0, 20.0, 0.0), glm::radians(180.0f), 0.0f);
    player.getCamera().setAspectRatio(wnd.getAspectRatio());
    player.getCamera().setFarPlane(CAMERA_FAR_PLANE);

    InputManager& playerInputManager = player.getInputManager();
    wnd.linkInputManager(&playerInputManager);

    // Input
    glm::vec2 previousMousePos{};
    wnd.getMousePos(previousMousePos.x, previousMousePos.y);
    glfwSetInputMode(wnd.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(wnd.getWindow(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // Timers
    double lastTime = glfwGetTime();
	UpdateTimer worldUpdateTimer(20.0); worldUpdateTimer.setUpdateToTrue();
	UpdateTimer profilerUpdateTimer(1.0 / 3.0);
    UpdateTimer frequentUIDataUpdateTimer(1.0);

    // Frequent UI data
    DebugUIMetrics uiMetrics;
    double accumulatedTime = 0.0f;
    int accumulatedFrames = 0;

    // Container UI
    ContainerUI containerUI;
    setupContainerUI(containerUI);

    // Main loop
    while (!wnd.shouldClose())
    {
		// Poll events
        wnd.pollEvents();
        
        // Check if window is iconified
        const bool iconified = wnd.isZeroSize();

		// Time logic
        // TODO: Maybe reset timer. Maybe if timer will get too big, everything will break.
        double time = glfwGetTime();
        double deltaTime = time - lastTime;
		lastTime = time;

        accumulatedTime += deltaTime;
        accumulatedFrames++;

		worldUpdateTimer.addTime(deltaTime);
		profilerUpdateTimer.addTime(deltaTime);
        frequentUIDataUpdateTimer.addTime(deltaTime);

        // Sounds
		SoundManager::getInstance().update();

        // World
        world.setAppTime(time);
        if (worldUpdateTimer.peek())
        {
            glm::dvec3 playerPos = player.getPosition();
            world.loadChunks(playerPos);

            while (worldUpdateTimer.shouldUpdate())
            {
                world.update(worldUpdateTimer.getUpdateInterval());
            }

            if (wnd.isKeyPressed(GLFW_KEY_P))
            {
                world.rebuildAllChunkMeshes();
                std::cout << "World: All chunks meshes are rebuild.\n";
            }

            if (wnd.isKeyPressed(GLFW_KEY_O))
            {
                world.debugMethod();
            }
        }
        world.sendChunkMeshesToGPU();

        //
        {
            static bool previousInventoryOpened = false;
            bool opened = player.isInventoryOpened();

            bool open = !previousInventoryOpened && opened;
            bool close = previousInventoryOpened && !opened;

            previousInventoryOpened = opened;

            if (open)
            {
                glfwSetInputMode(wnd.getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            else if (close)
            {
                glfwSetInputMode(wnd.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                int width, height;
                glfwGetWindowSize(wnd.getWindow(), &width, &height);
                glfwSetCursorPos(wnd.getWindow(), width / 2, height / 2);
            }
        }

        //
        if (iconified)
        {
            // Force app to 20 fps. Stop rendering and swapping buffers.
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 20));
        }
        else
        {
            // Camera
            player.interpolateCameraTransform(worldUpdateTimer.getAccumulatedTimeInPercent());
            player.getCamera().setAspectRatio(wnd.getAspectRatio());

            // Rendering world
            if (framebuffer.isComplete())
            {
                world.render(player.getCamera(), framebuffer, player.raycastResult);

                // Update UI metrics with world debug data
                const World::DebugData& worldDebug = world.getDebugData();
                uiMetrics.loadedChunksCount = worldDebug.loadedChunksCount;
                uiMetrics.renderedChunks = worldDebug.renderedChunks;
                uiMetrics.totalFaces = worldDebug.totalFaces;
                uiMetrics.totalFaceCapacityInBytes = worldDebug.totalFaceCapacityInBytes;
                uiMetrics.renderedFaceCount = worldDebug.renderedFaceCount;
                uiMetrics.chunkDrawCommandBufferSizeInBytes = worldDebug.chunkDrawCommandBufferSizeInBytes;
                uiMetrics.chunkPositionBufferSizeInBytes = worldDebug.chunkPositionBufferSizeInBytes;

                // Render UI
                const float aspectRatio = wnd.getAspectRatio();
                TextRenderer::setCustomCoordinateSpace(-aspectRatio, aspectRatio, -1.0f, 1.0f);
                renderUI(aspectRatio, containerUI, player);

                // Render UI text
                renderDebugData(wnd, player, uiMetrics);

                // Blitting FBO to default FBO
                framebuffer.blitToDefaultFramebuffer(wnd.getWidth(), wnd.getHeight());

                // Swap buffers
                wnd.swapBuffers();
            }
            else
            {
                std::cout << "[main]: FBO is not complete!\n";
            }
        }

        //
        if (frequentUIDataUpdateTimer.shouldUpdate())
        {
            uiMetrics.fps = accumulatedFrames / accumulatedTime;
            uiMetrics.frameTimeMs = accumulatedTime / accumulatedFrames * 1000.0;
            accumulatedTime = 0.0;
            accumulatedFrames = 0;
        }

        if (profilerUpdateTimer.shouldUpdate())
        {
            Profiler::printProfileReport();
        }
    }

    //Profiler::printProfileReport();

    glfwTerminate();
	return 0;
}