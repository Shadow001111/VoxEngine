#include "WindowManager.h"

#include <iostream>

WindowManager::WindowManager(const WindowParams& params)
{
    if (!glfwInit())
    {
        std::cerr << "[WindowManager]: Failed to initialize GLFW" << std::endl;
        return;
    }

    // OpenGL 4.6 core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, params.resizable);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, params.openglDebug);

    // Create window
    window = glfwCreateWindow(params.width, params.height, params.title.c_str(), nullptr, nullptr);
    if (!window)
    {
        std::cerr << "[WindowManager]: Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);

    // Load OpenGL via GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "[WindowManager]: Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    // Store pointer for callbacks
    glfwSetWindowUserPointer(window, this);

    // Setup callbacks
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);

    // vsync
    glfwSwapInterval(params.vsync);

    //
	width = params.width;
	height = params.height;
	aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    vsync = params.vsync;

    opaqueFramebuffer = std::make_unique<OpenGL_FBO>(width, height);
    opaqueFramebuffer->bind();
    opaqueFramebuffer->createColorAttachment("color", GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    opaqueFramebuffer->createDepthAttachment("depth", GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    opaqueFramebuffer->setupDrawBuffers();
    if (!opaqueFramebuffer->isComplete())
    {
        std::cerr << "[WindowManager]: Failed to create opaque framebuffer" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    translucentFramebuffer = std::make_unique<OpenGL_FBO>(width, height);
    translucentFramebuffer->bind();
    translucentFramebuffer->createColorAttachment("accumulation", GL_RGBA16F, GL_RGBA, GL_FLOAT);
    translucentFramebuffer->createColorAttachment("revealage", GL_R8, GL_RED, GL_FLOAT);
    translucentFramebuffer->linkDepthTexture("depth", *opaqueFramebuffer->getTexture("depth").value());
    translucentFramebuffer->setupDrawBuffers();
    if (!translucentFramebuffer->isComplete())
    {
        std::cerr << "[WindowManager]: Failed to create translucent framebuffer" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }
}

WindowManager::~WindowManager()
{
    if (window)
    {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

void WindowManager::pollEvents() const
{
    glfwPollEvents();
}

void WindowManager::swapBuffers() const
{
    glfwSwapBuffers(window);
}

bool WindowManager::shouldClose() const
{
    return glfwWindowShouldClose(window);
}

void WindowManager::setTitle(const std::string& title) const
{
    glfwSetWindowTitle(window, title.c_str());
}

GLFWwindow* WindowManager::getWindow() const
{
	return window;
}

int WindowManager::getWidth() const
{
    return width;
}

int WindowManager::getHeight() const
{
    return height;
}

float WindowManager::getAspectRatio() const
{
    return aspectRatio;
}

bool WindowManager::getVSYNC() const
{
    return vsync;
}

OpenGL_FBO* WindowManager::getOpaqueFBO() const
{
    return opaqueFramebuffer.get();
}

OpenGL_FBO* WindowManager::getTranslucentFBO() const
{
    return translucentFramebuffer.get();
}

bool WindowManager::isKeyPressed(int key) const
{
	return glfwGetKey(window, key) == GLFW_PRESS;
}

bool WindowManager::isMouseButtonPressed(int button) const
{
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
}

void WindowManager::getMousePos(float& xpos, float& ypos) const
{
    double xpos_,  ypos_;
	glfwGetCursorPos(window, &xpos_, &ypos_);
    xpos = static_cast<float>(xpos_);
	ypos = static_cast<float>(ypos_);
}

void WindowManager::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    auto* gm = static_cast<WindowManager*>(glfwGetWindowUserPointer(window));
	if (gm) gm->onResize(width, height);
}

void WindowManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* gm = static_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (gm) gm->onKey(key, scancode, action, mods);
}

void WindowManager::onResize(int width, int height)
{
    this->width = width;
    this->height = height;
    this->aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    glViewport(0, 0, width, height);

    opaqueFramebuffer->bind();
    opaqueFramebuffer->resize(width, height);

    translucentFramebuffer->bind();
    translucentFramebuffer->resize(width, height);
}

void WindowManager::onKey(int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}
