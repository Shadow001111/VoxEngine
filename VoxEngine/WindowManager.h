#pragma once
#include "OpenGLWrappers/OpenGL_FBO.h"
#include <GLFW/glfw3.h>
#include <memory>

struct WindowParams
{
    int width = 800;
    int height = 600;
    std::string title = "OpenGL Window";
    bool resizable = false;
    bool vsync = false;
    bool openglDebug = false;
};

class WindowManager
{
    GLFWwindow* window = nullptr;

    robin_hood::unordered_flat_set<OpenGL_FBO*> linkedFramebuffers;

    int width, height;
	float aspectRatio;
    bool vsync;
public:
    WindowManager(const WindowParams& params);
	~WindowManager();

	void pollEvents() const;
    void swapBuffers() const;
	bool shouldClose() const;

    // Framebuffer linkage (for resizing)
    void linkFramebuffer(OpenGL_FBO* fbo);
    void unlinkFramebuffer(OpenGL_FBO* fbo);
    bool isFramebufferLinked(OpenGL_FBO* fbo) const;

    // Setters
    void setTitle(const std::string& title) const;

	// Getters
    GLFWwindow* getWindow() const { return window; };
    int getWidth() const { return width; };
    int getHeight() const { return height; };
    float getAspectRatio() const { return aspectRatio; };
    bool getVSYNC() const { return vsync; };

    bool isIconified() const { return glfwGetWindowAttrib(window, GLFW_ICONIFIED); }
    bool isZeroSize() const { return !(width > 0 && height > 0); };

    // Keys and mouse
	bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
	void getMousePos(float& xpos, float& ypos) const;

    // OpenGL debug
    static void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length,
        const GLchar* message, const void* userParam);
    static void setOpenGLDebugMessageFilter(GLenum source = GL_DONT_CARE,
        GLenum type = GL_DEBUG_TYPE_OTHER,
        GLenum severity = GL_DONT_CARE,
        GLsizei count = 0,
        const GLuint* ids = nullptr,
        GLboolean enabled = GL_FALSE);

    // Static forwarding callbacks
	static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Instance-level callback handling
	void onResize(int width, int height);
    void onKey(int key, int scancode, int action, int mods);
};

