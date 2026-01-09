#pragma once
#include "OpenGLWrappers/OpenGL_FBO.h"
#include <GLFW/glfw3.h>
#include <memory>

// Struct to hold window initialization parameters
struct WindowParams
{
    int width = 800;
    int height = 600;
    std::string title = "OpenGL Window";
    bool resizable = true;
    bool vsync = false;
    bool openglDebug = false;
};

class WindowManager
{
    GLFWwindow* window = nullptr;
    //GLFWmonitor* monitor = nullptr;

    OpenGL_FBO framebuffer;

    int width, height;
	float aspectRatio;
    bool vsync;
public:
    WindowManager(const WindowParams& params);
	~WindowManager();

	void pollEvents() const;
    void swapBuffers() const;
	bool shouldClose() const;

    // Setters
    void setTitle(const std::string& title) const;

	// Getters
    GLFWwindow* getWindow() const { return window; };
    int getWidth() const { return width; };
    int getHeight() const { return height; };
    float getAspectRatio() const { return aspectRatio; };
    bool getVSYNC() const { return vsync; };
    const OpenGL_FBO& getFBO() const { return framebuffer; };

    bool isIconified() const { return glfwGetWindowAttrib(window, GLFW_ICONIFIED); }
    bool isZeroSize() const { return !(width > 0 && height > 0); };

    //
	bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
	void getMousePos(float& xpos, float& ypos) const;

    // Static forwarding callbacks
	static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Instance-level callback handling
	void onResize(int width, int height);
    void onKey(int key, int scancode, int action, int mods);
};

