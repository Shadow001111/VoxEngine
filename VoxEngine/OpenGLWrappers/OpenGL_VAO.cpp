#include "OpenGL_VAO.h"

#include "OpenGLWrappers/openGLDebug.h"

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
    glGenVertexArrays(1, &id);
}

void OpenGL_VAO::bind() const
{
    glBindVertexArray(id);
}

void OpenGL_VAO::unbind() const
{
    glBindVertexArray(0);
}

void OpenGL_VAO::setFloatAttribute(GLuint index, GLint componentCount, GLenum type,
    GLboolean normalized, GLsizei stride, const void* pointer)
{
    OPENGL_CHECK_BIND_TARGET(id, GL_VERTEX_ARRAY);
    glVertexAttribPointer(index, componentCount, type, normalized, stride, pointer);
}

void OpenGL_VAO::setIntAttribute(GLuint index, GLint componentCount, GLenum type,
    GLsizei stride, const void* pointer)
{
    OPENGL_CHECK_BIND_TARGET(id, GL_VERTEX_ARRAY);
    glVertexAttribIPointer(index, componentCount, type, stride, pointer);
}

void OpenGL_VAO::enableAttribute(GLuint index)
{
    OPENGL_CHECK_BIND_TARGET(id, GL_VERTEX_ARRAY);
    glEnableVertexAttribArray(index);
}

void OpenGL_VAO::disableAttribute(GLuint index)
{
    OPENGL_CHECK_BIND_TARGET(id, GL_VERTEX_ARRAY);
    glDisableVertexAttribArray(index);
}

void OpenGL_VAO::setAttributeDivisor(GLuint index, GLuint divisor)
{
    OPENGL_CHECK_BIND_TARGET(id, GL_VERTEX_ARRAY);
    glVertexAttribDivisor(index, divisor);
}