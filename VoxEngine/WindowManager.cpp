#include "WindowManager.h"

#include <stdexcept>

WindowManager::WindowManager(const WindowParams& params)
{
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    // OpenGL 4.6 core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, params.resizable ? GLFW_TRUE : GLFW_FALSE);

    // Create window
    window = glfwCreateWindow(params.width, params.height, params.title.c_str(), nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);

    // Load OpenGL via GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // Store pointer for callbacks
    glfwSetWindowUserPointer(window, this);

    // Setup callbacks
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);

    //
	this->width = params.width;
	this->height = params.height;
	this->aspectRatio = static_cast<float>(width) / static_cast<float>(height);
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

bool WindowManager::isKeyPressed(int key) const
{
	return glfwGetKey(window, key) == GLFW_PRESS;
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
    glViewport(0, 0, width, height);

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
}

void WindowManager::onKey(int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}
