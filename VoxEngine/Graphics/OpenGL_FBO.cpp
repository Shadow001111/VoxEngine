#include "OpenGL_FBO.h"

#include <stdexcept>

OpenGL_FBO::OpenGL_FBO(int width, int height)
{
	// Framebuffer
	glGenFramebuffers(1, &id);
	glBindFramebuffer(GL_FRAMEBUFFER, id);

	// Color texture
	glGenTextures(1, &colorTextureID);
	glBindTexture(GL_TEXTURE_2D, colorTextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTextureID, 0);

	// Depth texture
	glGenTextures(1, &depthTextureID);
	glBindTexture(GL_TEXTURE_2D, depthTextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTextureID, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("Framebuffer is not complete.");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

OpenGL_FBO::~OpenGL_FBO()
{
	if (id) glDeleteFramebuffers(1, &id);
	if (colorTextureID) glDeleteTextures(1, &colorTextureID);
	if (depthTextureID) glDeleteTextures(1, &depthTextureID);
}

OpenGL_FBO::OpenGL_FBO(OpenGL_FBO&& other) noexcept :
	id(other.id), colorTextureID(other.colorTextureID), depthTextureID(other.depthTextureID)
{
	other.id = other.colorTextureID = other.depthTextureID = 0;
}

OpenGL_FBO& OpenGL_FBO::operator=(OpenGL_FBO&& other) noexcept
{
	if (this != &other)
	{
		if (id) glDeleteFramebuffers(1, &id);
		if (colorTextureID) glDeleteTextures(1, &colorTextureID);
		if (depthTextureID) glDeleteTextures(1, &depthTextureID);

		id = other.id;
		colorTextureID = other.colorTextureID;
		depthTextureID = other.depthTextureID;

		other.id = other.colorTextureID = other.depthTextureID = 0;
	}
	return *this;
}

void OpenGL_FBO::bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, id);
}

void OpenGL_FBO::bindTextures() const
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, colorTextureID);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depthTextureID);
}

void OpenGL_FBO::unbind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGL_FBO::resize(int w, int h)
{
	// Recreate color texture
	glBindTexture(GL_TEXTURE_2D, colorTextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	// Recreate depth texture  
	glBindTexture(GL_TEXTURE_2D, depthTextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glBindTexture(GL_TEXTURE_2D, 0);
}
