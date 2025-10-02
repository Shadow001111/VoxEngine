#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <stdexcept>
#include <iostream>

// Struct to hold window initialization parameters
struct WindowParams
{
    int width = 800;
    int height = 600;
    std::string title = "OpenGL Window";
    bool resizable = true;
};

class WindowManager
{
    GLFWwindow* window = nullptr;
public:
    WindowManager(const WindowParams& params);
	~WindowManager();

	void pollEvents() const;
    void swapBuffers() const;
	bool shouldClose() const;
    GLFWwindow* getWindow() const;

    // Static forwarding callbacks
	static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Instance-level callback handling
    void onKey(int key, int scancode, int action, int mods);
};

