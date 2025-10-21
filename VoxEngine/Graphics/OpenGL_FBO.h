#pragma once
#include <glad/glad.h>

class OpenGL_FBO
{
	GLuint id;
	GLuint colorTextureID;
	GLuint depthTextureID;
public:
	OpenGL_FBO(int width, int height);
	~OpenGL_FBO();

	OpenGL_FBO(const OpenGL_FBO& other) = delete;
	OpenGL_FBO& operator=(const OpenGL_FBO& other) = delete;

	OpenGL_FBO(OpenGL_FBO&& other) noexcept;
	OpenGL_FBO& operator=(OpenGL_FBO&& other) noexcept;

	void bind() const;
	void bindTextures() const;
	static void unbind();

	void resize(int w, int h);
};

