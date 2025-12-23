#include "WindowManager.h"

#include "Core/UpdateTimer.h"
#include "Core/Profiler.h"

#include "Game/World.h"
#include "Game/World/Player.h"
#include "Game/DataPackManagment/AssetRegistry.h"

#include "Graphics/TextRenderer.h"
#include "Graphics/quad_vertices.h"
#include "Graphics/TextureLoader.h"

#include "OpenGLWrappers/OpenGL_VAO.h"

#include "SoundManager.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <glm/gtc/matrix_transform.hpp>

#ifdef NDEBUG
constexpr int CHUNK_LOAD_DISTANCE = 12;
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

static void setupFramebuffer(OpenGL_VAO& rectVAO, OpenGL_Buffer& rectVBO, std::unique_ptr<Shader>& shader)
{
    // Mesh
    rectVAO.bind();
    rectVBO.bind();
    rectVBO.allocateMemory(sizeof(quadVertices));
    rectVBO.write(quadVertices, sizeof(quadVertices));

    rectVAO.enableAttribute(0);
    rectVAO.setFloatAttribute(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // Shader
    std::vector<Shader::ShaderSource> fboShaderSources =
    {
        {GL_VERTEX_SHADER, "res/Shaders/quad.vert"},
        {GL_FRAGMENT_SHADER, "res/Shaders/fbo.frag"}
    };
    shader = std::make_unique<Shader>(fboShaderSources);
    shader->use();
    shader->setInt("colorTexture", 0);
    shader->setInt("depthTexture", 1);
}

static void setupOpenGLStates()
{
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}


static void collectPlayerInput(PlayerInput& input, const WindowManager& wnd, glm::vec2& previousMousePos)
{
    input.moveForward |= wnd.isKeyPressed(GLFW_KEY_W);
    input.moveBackward |= wnd.isKeyPressed(GLFW_KEY_S);
    input.moveLeft |= wnd.isKeyPressed(GLFW_KEY_A);
    input.moveRight |= wnd.isKeyPressed(GLFW_KEY_D);
    input.jump |= wnd.isKeyPressed(GLFW_KEY_SPACE);
    input.crouch |= wnd.isKeyPressed(GLFW_KEY_LEFT_CONTROL);
    input.sprint |= wnd.isKeyPressed(GLFW_KEY_LEFT_SHIFT);

    for (int i = 0; i <= 9; i++)
    {
        input.numbers[i] |= wnd.isKeyPressed(GLFW_KEY_0 + i);
    }

	static bool prevLeftMouseState = false;
	static bool prevRightMouseState = false;

    input.leftMousePressed |= wnd.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    input.rightMousePressed |= wnd.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);

    if (!prevLeftMouseState && input.leftMousePressed)
    {
		input.leftMouseClicked = true;
    }
    if (!prevRightMouseState && input.rightMousePressed)
	{
		input.rightMouseClicked = true;
	}

	prevLeftMouseState = input.leftMousePressed;
	prevRightMouseState = input.rightMousePressed;

    float mouseX, mouseY;
    wnd.getMousePos(mouseX, mouseY);
    input.mouseDelta += glm::vec2(mouseX - previousMousePos.x, mouseY - previousMousePos.y);
    previousMousePos = glm::vec2(mouseX, mouseY);
}

struct ContainerUI
{
    Shader hotbarShader;
    OpenGL_Texture hotbarSlotImage;
    OpenGL_VAO hotbarVAO;
    OpenGL_Buffer hotbarVBO{ GL_ARRAY_BUFFER, GL_STATIC_DRAW };

    OpenGL_Texture itemUITextureArray;
};

static void setupContainerUI(ContainerUI& c)
{
    {
        std::vector<Shader::ShaderSource> sources =
        {
            {GL_VERTEX_SHADER, "res/Shaders/hotbar.vert"},
            {GL_FRAGMENT_SHADER, "res/Shaders/hotbar.frag"}
        };
        c.hotbarShader = Shader(sources);
        c.hotbarShader.use();
        c.hotbarShader.setInt("uTexture", 0);
    }

    {
        c.hotbarVAO.bind();

        c.hotbarVBO.bind();
        c.hotbarVBO.allocateMemory(sizeof(quadVertices));
        c.hotbarVBO.write(quadVertices, sizeof(quadVertices));

        c.hotbarVAO.enableAttribute(0);
        c.hotbarVAO.setFloatAttribute(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    }

    {
        std::vector<std::string> itemTextureNames = AssetRegistry::getItemUITextureNames();

        TextureLoader::TextureParams params;

        PROFILE_SCOPE("Item ui texture array creation", ProfileCategory::General);
        TextureLoader::createAndLoadTextureArray(c.itemUITextureArray, "res/ItemUITextures", itemTextureNames, 128, params);
    }

    {
        TextureLoader::TextureParams params;

        TextureLoader::createAndLoadTextureArray(c.hotbarSlotImage, "res/UITextures", { "empty_hotbar_slot", "selected_hotbar_slot" }, 24, params);
    }
}

static void renderHotbar(const ContainerUI& c, const Player* player)
{
    const Item* hotbar = player->getHotbar();

    //
    constexpr int   slotCount = PLAYER_HOTBAR_SIZE;
    constexpr float slotSize = 0.2f;
    constexpr float bottomOffset = 0.0f;

    // Texture pixel sizes
    constexpr float TEXTURE_PX = 24.0f;
    constexpr float EMPTY_PX = 16.0f;
    constexpr float SELECTED_PX = 22.0f;
    constexpr float ITEM_PX = EMPTY_PX - 4.0f;

    // World-space scaling
    const float emptyScale = (TEXTURE_PX / EMPTY_PX) * slotSize;
    const float selectedScale = emptyScale;
    const float itemScale = (ITEM_PX / EMPTY_PX) * slotSize;

    float totalWidth = slotCount * slotSize;
    float startX = -totalWidth * 0.5f + slotSize * 0.5f;

    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float aspect = float(viewport[2]) / float(viewport[3]);

    glm::mat4 projection = glm::ortho(
        -aspect, aspect,
        -1.0f, 1.0f,
        -1.0f, 1.0f
    );

    c.hotbarVAO.bind();
    c.hotbarShader.use();
    c.hotbarShader.setMat4("projection", projection);

    /* =========================
       1. EMPTY HOTBAR SLOTS
       ========================= */
    c.hotbarSlotImage.bind(0);
    c.hotbarShader.setUint("uTextureId", 0); // array layer 0

    for (int i = 0; i < slotCount; i++)
    {
        float x = startX + i * slotSize;
        float y = -1.0f + bottomOffset + slotSize * 0.5f;

        glm::mat4 model(1.0f);
        model = glm::translate(model, { x, y, 0.0f });
        model = glm::scale(model, { emptyScale * 0.5f, emptyScale * 0.5f, 1.0f });

        c.hotbarShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    /* =========================
       2. ITEMS (24x24)
       ========================= */
    c.itemUITextureArray.bind(0);

    for (int i = 0; i < slotCount; i++)
    {
        const Item& item = hotbar[i];
        if (item.count == 0)
        {
            continue;
        }

        const auto* itemData = AssetRegistry::getItemData(item.id);
        if (!itemData)
        {
            continue;
        }

        float x = startX + i * slotSize;
        float y = -1.0f + bottomOffset + slotSize * 0.5f;

        glm::mat4 model(1.0f);
        model = glm::translate(model, { x, y, 0.0f });
        model = glm::scale(model, { itemScale * 0.5f, itemScale * 0.5f, 1.0f });

        c.hotbarShader.setMat4("model", model);
        c.hotbarShader.setUint("uTextureId", itemData->uiTextureId);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    /* =========================
       3. SELECTED OVERLAY
       ========================= */
    c.hotbarSlotImage.bind(0);
    c.hotbarShader.setUint("uTextureId", 1); // array layer 1 (selected)

    int selectedSlot = player->getSelectedItemIndex();
    {
        float x = startX + selectedSlot * slotSize;
        float y = -1.0f + bottomOffset + slotSize * 0.5f;

        glm::mat4 model(1.0f);
        model = glm::translate(model, { x, y, 0.0f });
        model = glm::scale(model, { selectedScale * 0.5f, selectedScale * 0.5f, 1.0f });

        c.hotbarShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}

static void renderUI(const ContainerUI& c, const Player* player)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderHotbar(c, player);
}

static void renderDebugData(const World::DebugData& debug, const WindowManager& wnd, Player* player, float FPS)
{
    float rowHeight = 24.0f;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);

    ss << "FPS: " << FPS;

    if (wnd.getVSYNC())
    {
        ss << " VSYNC";
    }

    // Chunks
    ss << "\nChunks: Loaded: " << formatSize(debug.loadedChunksCount)
        << ", Rendered: " << formatSize(debug.renderedChunks);

    // Faces
    ss << "\nFaces: " << formatSize(debug.totalFaces)
        << "/" << formatSize(debug.totalFaceCapacityInBytes)
        << ", Rendered: " << formatSize(debug.renderedFaceCount);

    // Meshes
    ss << "\nChunk meshes: Capacity: " << formatSizeBinary(debug.totalFaceCapacityInBytes);

    // Buffer sizes
    ss << "\nChunk draw command buffer: " << formatSizeBinary(debug.chunkDrawCommandBufferSizeInBytes);
    ss << "\nChunk position buffer: " << formatSizeBinary(debug.chunkPositionBufferSizeInBytes);

    // TODO: Add textures and font size in bytes

    // Player orientation
    const Camera& camera = player->getCamera();
    const auto& playerPos = player->getPosition();
    const auto& cameraViewDirection = camera.getForward();

    glm::ivec3 localPlayerPos = glm::ivec3(glm::mod(glm::mod(playerPos, (double)CHUNK_SIZE) + (double)CHUNK_SIZE, (double)CHUNK_SIZE));

    ss << "\nXYZ: " << playerPos.x << " " << playerPos.y << " " << playerPos.z;
    ss << "\nBlock: " << localPlayerPos.x << " " << localPlayerPos.y << " " << localPlayerPos.z;

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


void APIENTRY glDebugOutput(GLenum source,
    GLenum type,
    unsigned int id,
    GLenum severity,
    GLsizei length,
    const char* message,
    const void* userParam)
{
    // Ignore
    if (
        // Non-significant error/warning codes
        id == 131169 || id == 131185 || id == 131218 || id == 131204 ||
		// Pixel sync
        id == 131154
        ) return;

    std::cout << "---------------\n";
    std::cout << "Debug message (" << id << "): " << message << "\n";

    switch (source)
    {
    case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
    case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
    } std::cout << "\n";

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
    } std::cout << "\n";

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
    case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
    } std::cout << "\n";
    std::cout << std::endl;
}

// TODO: Modern OpenGl
int main()
{
    constexpr float CAMERA_FAR_PLANE = (CHUNK_LOAD_DISTANCE + 0.5f) * (CHUNK_SIZE * 1.41f);

    //
    std::ios_base::sync_with_stdio(false);

    // Window
    WindowManager wnd({ 1280, 720, "VoxEngine", true, true, true });

    // OpenGL debug
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    glDebugMessageControl(
        GL_DONT_CARE,          // any source
        GL_DEBUG_TYPE_OTHER,   // filter this type
        GL_DONT_CARE,          // any severity
        0, NULL,               // no specific IDs
        GL_FALSE               // disable
    );

    // Framebuffer
    auto* FBO = wnd.getOpaqueFBO();
    OpenGL_VAO rectVAO;
    OpenGL_Buffer rectVBO(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    std::unique_ptr<Shader> fboShader;
    setupFramebuffer(rectVAO, rectVBO, fboShader);

    // OpenGL states
    setupOpenGLStates();

    // Text renderer
    TextRenderer::init();
    TextRenderer::loadFont("RusEngMinecraft", 8);
    TextRenderer::setCurrentFont("RusEngMinecraft");

    // World
    World world;
    world.setChunkLoadingDistance(CHUNK_LOAD_DISTANCE);
    world.preparation();

    // Player
    Player* player = world.createEntity<Player>(glm::vec3(0, 20.0, 0.0), glm::radians(180.0f), 0.0f);
    player->getCamera().setAspectRatio(wnd.getAspectRatio());
    player->getCamera().setFarPlane(CAMERA_FAR_PLANE);

    PlayerInput playerInput;

    // Input
    glm::vec2 previousMousePos;
    wnd.getMousePos(previousMousePos.x, previousMousePos.y);
    glfwSetInputMode(wnd.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Timers
    double lastTime = glfwGetTime();
	UpdateTimer worldUpdateTimer(20.0f); worldUpdateTimer.setUpdateToTrue();
	UpdateTimer profilerUpdateTimer(1.0f / 3.0f);
    UpdateTimer frequentUIDataUpdateTimer(1.0f);

    // Frequent UI data
    float UI_FPS = 0.0f;
    float accumulatedTime = 0.0f;
    int accumulatedFrames = 0;

    // Container UI
    ContainerUI containerUI;
    setupContainerUI(containerUI);

    // Main loop
    while (!wnd.shouldClose())
    {
		// Poll events
        wnd.pollEvents();

		// Time logic
        // TODO: Maybe reset timer. Maybe if timer will get too big, everything will break.
        double time = glfwGetTime();
        double deltaTime = time - lastTime;
		lastTime = time;

        accumulatedTime += (float)deltaTime;
        accumulatedFrames++;

		worldUpdateTimer.addTime(deltaTime);
		profilerUpdateTimer.addTime(deltaTime);
        frequentUIDataUpdateTimer.addTime(deltaTime);

        // Sounds
		SoundManager::getInstance().update();

        // Player
        collectPlayerInput(player->input, wnd, previousMousePos);

        // World
        world.setAppTime(time);
        if (worldUpdateTimer.peek())
        {
            glm::vec3 playerPos = player->getPosition();
            world.loadChunks(playerPos);

            while (worldUpdateTimer.shouldUpdate())
            {
                world.update(worldUpdateTimer.getUpdateInterval());
            }

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

        player->interpolateCameraTransform(worldUpdateTimer.getAccumulatedTimeInPercent());

        world.sendChunkMeshesToGPU();

        // Rendering world
        world.renderBackround(player->getCamera(), wnd.getOpaqueFBO());
        world.renderChunks(player->getCamera(), wnd.getOpaqueFBO(), wnd.getTranslucentFBO());
        world.renderVoxelMarker(player->getCamera(), player->raycastResult);
        renderUI(containerUI, player);
        wnd.getOpaqueFBO()->unbind();

        // Rendering to screen
        // TODO: Maybe we are rendering same FBO twice. Avoid that.
        rectVAO.bind();
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        //FBO->bind();
        FBO->bindTexture("color", 0);
        FBO->bindTexture("depth", 1);

        fboShader->use();

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // Text rendering
        TextRenderer::updateProjectionMatrix(wnd.getWidth(), wnd.getHeight());
        renderDebugData(world.getDebugData(), wnd, player, UI_FPS);

        // Swap buffers
        wnd.swapBuffers();

        //
        if (frequentUIDataUpdateTimer.shouldUpdate())
        {
            UI_FPS = accumulatedFrames / accumulatedTime;
            accumulatedTime = 0.0f;
            accumulatedFrames = 0;
        }

        if (profilerUpdateTimer.shouldUpdate())
        {
            Profiler::printProfileReport();
        }
    }

    Profiler::printProfileReport();

    while (GLenum err = glGetError() != GL_NO_ERROR)
    {
        std::cerr << "[OpenGl Error]: " << err << "\n";
    }

    glfwTerminate();
	return 0;
}