#include "OpenGL_SSBO.h"

OpenGL_SSBO::OpenGL_SSBO(GLuint bindingPoint) :
	OpenGL_Buffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW),
	bindingPoint(bindingPoint)
{
}

void OpenGL_SSBO::bindBase() const
{
	glBindBufferBase(target, bindingPoint, id);
}
