#include "OpenGL_VAO.h"

OpenGL_VAO::~OpenGL_VAO()
{
    if (id) glDeleteVertexArrays(1, &id);
}

OpenGL_VAO::OpenGL_VAO(OpenGL_VAO&& other) noexcept
    : id(other.id)
{
    other.id = 0;
}

OpenGL_VAO& OpenGL_VAO::operator=(OpenGL_VAO&& other) noexcept
{
    if (this != &other)
    {
        if (id) glDeleteVertexArrays(1, &id);

        id = other.id;

        other.id = 0;
    }
    return *this;
}

void OpenGL_VAO::create()
{
    if (id) glDeleteVertexArrays(1, &id);
    glCreateVertexArrays(1, &id);
}

void OpenGL_VAO::destroy()
{
    if (id) glDeleteVertexArrays(1, &id);
}

void OpenGL_VAO::bind() const
{
    glBindVertexArray(id);
}

void OpenGL_VAO::unbind()
{
    glBindVertexArray(0);
}

void OpenGL_VAO::setFloatAttribute(
    GLuint attributeIndex, GLint componentCount, GLsizei stride, GLuint bindingIndex,
    GLenum type, GLboolean normalized)
{
    glVertexArrayAttribFormat(id, attributeIndex, componentCount, type, normalized, stride);
    glVertexArrayAttribBinding(id, attributeIndex, bindingIndex);
}

void OpenGL_VAO::setIntAttribute(
    GLuint attributeIndex, GLint componentCount, GLsizei stride, GLuint bindingIndex,
    GLenum type)
{
    glVertexArrayAttribIFormat(id, attributeIndex, componentCount, type, stride);
    glVertexArrayAttribBinding(id, attributeIndex, bindingIndex);
}

void OpenGL_VAO::enableAttribute(GLuint index)
{
    glEnableVertexArrayAttrib(id, index);
}

void OpenGL_VAO::disableAttribute(GLuint index)
{
    glDisableVertexArrayAttrib(id, index);
}

void OpenGL_VAO::setAttributeDivisor(GLuint index, GLuint divisor)
{
    glVertexArrayBindingDivisor(id, index, divisor);
}

void OpenGL_VAO::bindVertexBuffer(GLuint bindingIndex, GLuint bufferId, GLintptr offset, GLsizei stride)
{
    glVertexArrayVertexBuffer(id, bindingIndex, bufferId, offset, stride);
}

void OpenGL_VAO::bindElementBuffer(GLuint bufferId)
{
    glVertexArrayElementBuffer(id, bufferId);
}
