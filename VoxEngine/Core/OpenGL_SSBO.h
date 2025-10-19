#pragma once
#include "OpenGL_Buffer.h"

class OpenGL_SSBO : public OpenGL_Buffer
{
	GLuint bindingPoint;
public:
	OpenGL_SSBO(GLuint bindingPoint);

	OpenGL_SSBO(const OpenGL_SSBO& other) = delete;
	OpenGL_SSBO& operator=(const OpenGL_SSBO& other) = delete;
	OpenGL_SSBO(OpenGL_SSBO&& other) = delete;
	OpenGL_SSBO& operator=(OpenGL_SSBO&& other) = delete;

	void bindBase() const;
};

