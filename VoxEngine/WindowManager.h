#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

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

    int width, height;
	float aspectRatio;
public:
    WindowManager(const WindowParams& params);
	~WindowManager();

	void pollEvents() const;
    void swapBuffers() const;
	bool shouldClose() const;

    // Setters
    void setTitle(const std::string& title) const;

	// Getters
    GLFWwindow* getWindow() const;
    int getWidth() const;
    int getHeight() const;
    float getAspectRatio() const;

    //
	bool isKeyPressed(int key) const;
	void getMousePos(float& xpos, float& ypos) const;

    // Static forwarding callbacks
	static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Instance-level callback handling
	void onResize(int width, int height);
    void onKey(int key, int scancode, int action, int mods);
};

