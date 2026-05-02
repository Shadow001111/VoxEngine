#include "WindowManager.h"

#include "Core/UpdateTimer.h"
#include "Game/TracyProfiler.h"

#include "Game/World.h"
#include "Game/Player/Player.h"
#include "Game/DataPackManagment/AssetRegistry.h"

#include "Graphics/TextRenderer.h"
#include "Graphics/quad_vertices.h"
#include "Graphics/TextureLoader.h"

#include "OpenGLWrappers/VertexArray.h"
#include "OpenGLWrappers/ImmutableBuffer.h"

#include "Game/SoundManager.h"

#include "FileLogger.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <glm/gtc/matrix_transform.hpp>
#include <json.hpp>

using json = nlohmann::json;

static std::string formatSize(size_t value)
{
    static const char* suffixes[] = { "", "k", "M", "G", "T", "P", "E" };
    constexpr size_t suffixCount = sizeof(suffixes) / sizeof(suffixes[0]);
    double scaled = static_cast<double>(value);
    size_t suffixIndex = 0;

    while (scaled >= 1000.0 && suffixIndex < suffixCount - 1)
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
    constexpr size_t suffixCount = sizeof(suffixes) / sizeof(suffixes[0]);
    double scaled = static_cast<double>(value);
    size_t suffixIndex = 0;

    while (scaled >= 1024.0 && suffixIndex < suffixCount - 1)
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
    FrameBuffer* fbo = nullptr;

    // Inventory
    Shader hotbarShader;
    Texture hotbarSlotImage;
    VertexArray hotbarVAO;
    ImmutableBuffer hotbarVBO;

    Texture itemUITextureArray;

    // Depth-buffer display
    VertexArray dbdVAO;
    ImmutableBuffer dbdVBO;
    Shader dbdShader;
};

struct DebugUIMetrics
{
    double fps = 0.0;
    double frameTimeMs = 0.0;  // Accumulated milliseconds per frame

	World::DebugData worldDebugData;
};

struct PlayerLoadConfig
{
	uint32_t chunkLoadingDistance = 8;
};

static void setupContainerUI(ContainerUI& c)
{
    TRACY_SCOPE_NC("Setup container ui", ProfileCategory::General);

    // Hotbar
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

    TextureLoader::TextureLoadParams textureLoadParametrs;
    textureLoadParametrs.desiredChannels = 4;
    textureLoadParametrs.compression = TextureCompression::Format::AUTO;

    Texture::Parameters textureParametrs
    {
        .minFilter = GL_NEAREST,
        .magFilter = GL_NEAREST,
        .wrapS = GL_CLAMP_TO_EDGE,
        .wrapT = GL_CLAMP_TO_EDGE
    };

    {
        std::vector<std::string> itemTextureNames = AssetRegistry::getItemUITextureNames();

        TextureLoader::createTextureArrayFromImages(c.itemUITextureArray, "res/ItemUITextures", itemTextureNames, textureLoadParametrs);

        c.itemUITextureArray.setParameters(textureParametrs);
    }

    {
        TextureLoader::createTextureArrayFromImages(c.hotbarSlotImage, "res/UITextures", { "empty_hotbar_slot", "selected_hotbar_slot" }, textureLoadParametrs);

        c.hotbarSlotImage.setParameters(textureParametrs);
    }

    // Depth-buffer display
    {
        std::vector<Shader::ShaderSource> sources =
        {
            {GL_VERTEX_SHADER, "res/Shaders/quad.vert"},
            {GL_FRAGMENT_SHADER, "res/Shaders/depthBufferDisplay.frag"}
        };
        c.dbdShader.create(sources);
        c.dbdShader.setInt("sampleTexture", 0);
    }

    {
        c.dbdVAO.create();

        c.dbdVBO.create(GL_ARRAY_BUFFER);
        c.dbdVBO.allocateStorage(sizeof(quadVertices), 0, quadVertices);

        c.dbdVAO.bindVertexBuffer(0, c.dbdVBO.getID(), 0, 4 * sizeof(float));

        c.dbdVAO.enableAttribute(0);
        c.dbdVAO.setFloatAttribute(0, 4, 0);
    }
}

static void renderInventory(const float aspectRatio, const GUIInventory& inventory, const ContainerUI& c)
{
    // Settings
    constexpr float TEXTURE_PX = 24.0f;
    constexpr float EMPTY_PX = 16.0f;
    //constexpr float SELECTED_PX = 22.0f;
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
    //const float selectedScale = emptyScale;
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

static void renderHotbarAndInventory(const float aspectRatio, const ContainerUI& c, const Player& player)
{
    renderInventory(aspectRatio, player.getHotbar(), c); // TODO: Add rendering choosen slot
    if (player.isInventoryOpened())
    {
        renderInventory(aspectRatio, player.getInventory(), c);
    }
}

//static void renderDepthBuffer(const float aspectRatio, const ContainerUI& c, const Player& player)
//{
//    c.dbdVAO.bind();
//    c.dbdShader.use();
//
//    auto textureOpt = c.fbo->getTexture("depth");
//    if (!textureOpt.has_value())
//    {
//        return;
//    }
//
//    const auto* texture = textureOpt.value();
//
//    if (Texture::getExtensions().bindless)
//    {
//        c.dbdShader.setHandleui64ARB("sampleTexture", texture->getHandle());
//    }
//    else
//    {
//        texture->bindUnit(0);
//    }
//
//    const float scale = 0.3f;
//    
//    float newWidth = aspectRatio * scale;
//    float quadRightBorder = newWidth;
//    float translateXBy = (aspectRatio - quadRightBorder);
//    float translateYBy = 1.0f - scale;
//
//    glm::mat4 model = glm::identity<glm::mat4>();
//    model = glm::translate(model, glm::vec3(translateXBy, translateYBy, 0.0f));
//    model = glm::scale(model, glm::vec3(aspectRatio * scale, scale, 1.0f));
//
//    glm::mat4 proj = glm::ortho(-aspectRatio, aspectRatio, -1.0f, 1.0f);
//
//    c.dbdShader.setMat4("model", model);
//    c.dbdShader.setMat4("projection", proj);
//
//    const auto& camera = player.getCamera();
//    float near = camera.getNear();
//    float far = camera.getFar();
//
//    c.dbdShader.setVec2("nearFar", near, far);
//
//    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
//}

static void renderUI(const float aspectRatio, const ContainerUI& c, const Player& player)
{
    TRACY_SCOPE_NC("Render UI", ProfileCategory::Render);

    //glDisable(GL_BLEND);
    //renderDepthBuffer(aspectRatio, c, player);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderHotbarAndInventory(aspectRatio, c, player);
}

static void renderDebugData(const WindowManager& wnd, const Player& player, const DebugUIMetrics& metrics)
{
    TRACY_SCOPE_NC("Render debug data", ProfileCategory::Render);

    // Get data refs
	const auto& worldData = metrics.worldDebugData;
	const auto& renderStats = worldData.renderStats;

    //
    const float rowHeight = 0.06f;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);

    ss << "FPS: " << metrics.fps << " (" << metrics.frameTimeMs << " ms)";

    if (wnd.getVSYNC())
    {
        ss << " VSYNC";
    }

    // Chunks
    ss << "\nChunks: Loaded: " << formatSize(worldData.chunkCount)
        << ", Rendered: " << formatSize(renderStats.renderedChunkCount);

    // Chunk regions
    ss << "\nChunk regions: " << worldData.chunkRegionCount;

    // Faces
    ss << "\nFaces: " << formatSize(worldData.chunkFaceCount)
        << "/" << formatSize(worldData.chunkFaceCapacity)
        << ", Rendered: " << formatSize(renderStats.renderedChunkFaceCount);

    // Meshes
    ss << "\nChunk meshes capacity: " << formatSizeBinary(worldData.chunkFaceCapacityInBytes);

    // Huge memory
    ss << "\nChunk draw command buffer: " << formatSizeBinary(renderStats.chunkDrawCommandBufferSizeInBytes);
    ss << "\nChunk position buffer: " << formatSizeBinary(renderStats.chunkPositionBufferSizeInBytes);
    ss << "\nChunk light queues' size: " << formatSizeBinary(worldData.chunkLightQueuesSizeInBytes);

    // Player orientation
    //const Camera& camera = player.getCamera();
    const auto& playerPos = player.getPosition();
    //const auto& cameraViewDirection = camera.getForward();

    glm::ivec3 localPlayerPos = glm::ivec3(glm::mod(glm::mod(playerPos, (double)CHUNK_SIZE) + (double)CHUNK_SIZE, (double)CHUNK_SIZE));

    ss << "\nXYZ: " << playerPos.x << " " << playerPos.y << " " << playerPos.z;
    ss << "\nBlock: " << localPlayerPos.x << " " << localPlayerPos.y << " " << localPlayerPos.z;

    //std::string facingDir;
    //{
    //    float absX = std::abs(cameraViewDirection.x);
    //    float absY = std::abs(cameraViewDirection.y);
    //    float absZ = std::abs(cameraViewDirection.z);
    //    if (absX > absY && absX > absZ) {
    //        facingDir = (cameraViewDirection.x > 0.0f) ? "+X" : "-X";
    //    }
    //    else if (absY > absX && absY > absZ) {
    //        facingDir = (cameraViewDirection.y > 0.0f) ? "+Y" : "-Y";
    //    }
    //    else {
    //        facingDir = (cameraViewDirection.z > 0.0f) ? "+Z" : "-Z";
    //    }
    //}
    //ss << "\nView direction: " << facingDir;

    // Threading
    //const auto& threadPool = ParallelUtils::getGlobalThreadPool();
    //ss << "\nTotal task count:" << formatSize(threadPool.getTaskTotalCount()) << "\n";

    // Render text
    std::string text = ss.str();

    TextRenderer::startTextRendering();
    TextRenderer::renderText(text, -wnd.getAspectRatio() + 0.014f, 1.0f - 0.014f, rowHeight, glm::vec3(1.0f, 0.0f, 0.0f));

    glDepthMask(GL_TRUE);
}

static void check()
{
    // Get data
    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    GLint maxArrayTextureLayers;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);

    GLint maxColorAttachments;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);

    GLint maxComputeTextureUnits;
    glGetIntegerv(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, &maxComputeTextureUnits);

    // Display
    std::cout << std::string(100, '=') << "\n";
    std::cout << "Data:\n";
    std::cout << "    OpenGL Version: " << glGetString(GL_VERSION)  << "\n";
    std::cout << "    GPU Vendor: " << glGetString(GL_VENDOR)       << "\n";
    std::cout << "    Renderer: " << glGetString(GL_RENDERER)       << "\n";

	bool isBindlessSupported = Texture::getExtensions().bindless;
	std::cout << "Extensions:\n";
	std::cout << "    Bindless textures: " << (isBindlessSupported ? "Supported" : "Not supported") << "\n";

	std::cout << "Texture-related:\n";
    std::cout << "    Max size: " << maxTextureSize << "\n";
    std::cout << "    Max anisotropy: " << Texture::getGlobalData().maxAnisotropy << "\n";
    std::cout << "    Max texture array layers: " << maxArrayTextureLayers << "\n";
    std::cout << "    Max compute texture units: " << maxComputeTextureUnits << "\n";

    std::cout << "Framebuffer-related:\n";
    std::cout << "    Max color attachment count: " << maxColorAttachments << "\n";

    // Check texture compression available
    {
        const auto& s = TextureCompression::getSupport();
        std::cout << "Texture compression support:\n"
            << "    S3TC (BC1/BC3) : " << (s.s3tc ? "yes" : "no") << "\n"
            << "    RGTC (BC4/BC5) : " << (s.rgtc ? "yes" : "no") << "\n"
            << "    BPTC (BC6/BC7) : " << (s.bptc ? "yes" : "no") << "\n"
            << "    ASTC           : " << (s.astc ? "yes" : "no") << "\n";
    }

    //
    std::cout << std::string(100, '=') << "\n";
}

PlayerLoadConfig loadPlayerLoadConfig()
{
	const fs::path kConfigPath = "res/configs/player_load_config.json";

    std::error_code ec;
    fs::create_directories(kConfigPath.parent_path(), ec);   // ensure folder exists

    PlayerLoadConfig config;
    json cfg;
    bool needsWrite = false;

    // Try to load existing config
    std::ifstream in(kConfigPath);
    if (in.is_open())
    {
        try
        {
            in >> cfg;

            // Validate 'chunkLoadingDistance'
            if (cfg.contains("chunkLoadingDistance") && cfg["chunkLoadingDistance"].is_number_integer())
            {
                int rawValue = cfg["chunkLoadingDistance"].get<int>();
                // Clamp to reasonable range for uint32_t (non‑negative, <= max)
                if (rawValue >= 0 && static_cast<uint64_t>(rawValue) <= UINT32_MAX)
                    config.chunkLoadingDistance = static_cast<uint32_t>(rawValue);
                else
					needsWrite = true;
            }
            else
            {
                needsWrite = true;
            }
        }
        catch (const json::exception&)
        {
            needsWrite = true;
        }
    }
    else
    {
        needsWrite = true;
    }

    // If we created a default or repaired a corrupted file, write it back
    if (needsWrite)
    {
        cfg = json::object();
        cfg["chunkLoadingDistance"] = config.chunkLoadingDistance;
        std::ofstream out(kConfigPath, std::ios::trunc);
        if (out.is_open())
        {
            out << cfg.dump(4) << '\n';
        }
    }

    return config;
}

static int gameFunc()
{
    // Window
    WindowManager wnd({
        .width = 1600,
        .height = 900,
        .title = "VoxEngine",
        .nativeFullscreen = false,
        .bolderlessFullscreen = false,
        .resizable = false,
        .vsync = true,
        .openglDebug = true,
        .strictAspectRatio = true
        });

    // Init texture global data
    {
        TRACY_SCOPE_NC("Init texture global data", ProfileCategory::General);
        Texture::initGlobalData();
    }

    // SoundManager
    auto& soundManager = SoundManager::getInstance();

    // Check
    check();

    // Create framebuffer
    FrameBuffer framebuffer;
    {
        TRACY_SCOPE_NC("Create framebuffer", ProfileCategory::General);

        framebuffer.create(wnd.getWidth(), wnd.getHeight());

        const Texture::Parameters params
        {
			.minFilter = GL_NEAREST,
			.magFilter = GL_NEAREST,
			.wrapS = GL_CLAMP_TO_EDGE,
			.wrapT = GL_CLAMP_TO_EDGE
        };
        const bool bindless = Texture::getExtensions().bindless;

        framebuffer.createColorAttachment("color", GL_RGBA8, params, bindless);
        framebuffer.createColorAttachment("geometryAlpha", GL_R8, params, bindless);
        framebuffer.createColorAttachment("accumulation", GL_RGBA16F, params, bindless);
        framebuffer.createColorAttachment("revealage", GL_R8, params, bindless);

        // TODO: Render aurora in lower resolution
        // Problem: Geometry alpha mask is in higher resolution, meaning we need to sample multiple pixels to check if geometry covers aurora or not.
        framebuffer.createStandaloneTextureAttachment("aurora", GL_RGBA8, params, 1.0f, bindless);

        framebuffer.createDepthAttachment("depth", GL_DEPTH_COMPONENT32F, params, bindless);

        if (!framebuffer.isComplete())
        {
            std::cerr << "[main]: Failed to create framebuffer\n";
            return -1;
        }

        wnd.linkFramebuffer(&framebuffer);

        framebuffer.bind();
    }

    // OpenGL states
    {
        TRACY_SCOPE_NC("Setup opengl states", ProfileCategory::General);
        setupOpenGLStates();
    }

    // Text renderer
    TextRenderer::init();
    TextRenderer::loadFont("RusEngMinecraft", 8);
    TextRenderer::setCurrentFont("RusEngMinecraft");
    TextRenderer::setGlyphInstanceBatchSize(1024);

	// Load player load config
	PlayerLoadConfig playerLoadConfig = loadPlayerLoadConfig();

    // World
    World world;
    world.setChunkLoadingDistance(playerLoadConfig.chunkLoadingDistance);
    world.preparation();

    // Player
    Player& player = *world.createEntity<Player>(glm::vec3(0.0, 20.0, 0.0), glm::radians(180.0f), 0.0f);
    player.getCamera().setAspectRatio(wnd.getAspectRatio());
    player.getCamera().setFarPlane(world.getPlayerCameraFarPlaneDistance());

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
    UpdateTimer profilerUpdateTimer(1.0 / 2.0);
    UpdateTimer frequentUIDataUpdateTimer(1.0);

    // Frequent UI data
    DebugUIMetrics uiMetrics;
    double accumulatedTime = 0.0f;
    int accumulatedFrames = 0;

    // Container UI
    ContainerUI containerUI;
    containerUI.fbo = &framebuffer;
    setupContainerUI(containerUI);

    // Main loop
    while (!wnd.shouldClose())
    {
        // Poll events
        wnd.pollEvents();

        // Check if window is iconified
        const bool iconified = wnd.isZeroSize();

        // Time logic
        double time = glfwGetTime();
        double deltaTime = time - lastTime;
        lastTime = time;

        accumulatedTime += deltaTime;
        accumulatedFrames++;

        worldUpdateTimer.addTime(deltaTime);
        profilerUpdateTimer.addTime(deltaTime);
        frequentUIDataUpdateTimer.addTime(deltaTime);

        // Sounds
        soundManager.update();

        // World
        world.setAppTime((float)time);
        if (playerInputManager.isKeyJustPressed(GLFW_KEY_F3))
        {
            world.rebuildAllChunkMeshes();
		}
        if (worldUpdateTimer.peek())
        {
            glm::dvec3 playerPos = player.getPosition();
            world.loadChunksAroundPlayer(playerPos);

            const float updateInterval = worldUpdateTimer.getUpdateInterval();
            while (worldUpdateTimer.shouldUpdate())
            {
                world.update(updateInterval);
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
                glfwSetCursorPos(wnd.getWindow(), (float)width * 0.5f, (float)height * 0.5f);
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
                framebuffer.setDrawBuffers({ "color", "geometryAlpha", "accumulation", "revealage" });

                world.render(player.getCamera(), framebuffer, player.raycastResult);

                // Update UI metrics with world debug data
                uiMetrics.worldDebugData = world.getDebugData();

                // Render UI
                const float aspectRatio = wnd.getAspectRatio();
                TextRenderer::setCustomCoordinateSpace(-aspectRatio, aspectRatio, -1.0f, 1.0f);

                framebuffer.setDrawBuffers({ "color" });

                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);

                renderUI(aspectRatio, containerUI, player);

                // Render UI text
                renderDebugData(wnd, player, uiMetrics);

                // Blitting FBO to default FBO
				framebuffer.setReadBuffer("color");
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
#ifdef TRACY_ENABLE
            FrameMark;
#endif
        }
    }
    return 0;
}

// TODO: Fix terrain generation at far lands
int main()
{
    std::ios_base::sync_with_stdio(false);

    int result;
    
    try
    {
        result = gameFunc();
    }
    catch (const std::exception& e)
    {
        FileLogger logger("log/crash.txt");

		std::string message = "EXCEPTION: " + std::string(e.what());

        logger.add(message);
        result = -1;
    }

    return result;
}