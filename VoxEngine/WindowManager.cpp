#include "WindowManager.h"

#include <iostream>

WindowManager::WindowManager(const WindowParams& params)
{
    if (!glfwInit())
    {
        std::cerr << "[WindowManager]: Failed to initialize GLFW\n";
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
        std::cerr << "[WindowManager]: Failed to create GLFW window\n";
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);

    // Load OpenGL via GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "[WindowManager]: Failed to initialize GLAD\n";
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

void WindowManager::linkFramebuffer(OpenGL_FBO* fbo)
{
    if (fbo == nullptr)
    {
        std::cout << "[WindowManager][linkFramebuffer]: Cannot link nullptr\n";
        return;
    }
    linkedFramebuffers.insert(fbo);

    fbo->resize(width, height);
}

void WindowManager::unlinkFramebuffer(OpenGL_FBO* fbo)
{
    linkedFramebuffers.erase(fbo);
}

bool WindowManager::isFramebufferLinked(OpenGL_FBO* fbo) const
{
    return linkedFramebuffers.contains(fbo);
}

void WindowManager::setTitle(const std::string& title) const
{
    glfwSetWindowTitle(window, title.c_str());
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

    if (width > 0 && height > 0)
    {
        glViewport(0, 0, width, height);

        OpenGL_FBO* lastFramebuffer = nullptr;
        for (auto* framebuffer : linkedFramebuffers)
        {
            framebuffer->bind();
            framebuffer->resize(width, height);
            lastFramebuffer = framebuffer;
        }
        if (lastFramebuffer)
        {
            lastFramebuffer->unbind();
        }
    }
}

void WindowManager::onKey(int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}
