#include "OpenGL_VAO.h"
#include "Core/Assert.h"

OpenGL_VAO::OpenGL_VAO()
{
    glGenVertexArrays(1, &id);
}

OpenGL_VAO::~OpenGL_VAO()
{
    if (id)
    {
        glDeleteVertexArrays(1, &id);
        id = 0;
    }
}

OpenGL_VAO::OpenGL_VAO(OpenGL_VAO&& other) noexcept
    : id(other.id), vertexSize(other.vertexSize), attributeCount(other.attributeCount)
{
    // Leave 'other' in a valid but empty state
    other.id = 0;
    other.vertexSize = 0;
    other.attributeCount = 0;
}

OpenGL_VAO& OpenGL_VAO::operator=(OpenGL_VAO&& other) noexcept
{
    if (this != &other)
    {
        // Delete current VAO if valid
        if (id)
        {
            glDeleteVertexArrays(1, &id);
        }

        // Move fields
        id = other.id;
        vertexSize = other.vertexSize;
        attributeCount = other.attributeCount;

        // Reset source
        other.id = 0;
        other.vertexSize = 0;
        other.attributeCount = 0;
    }
    return *this;
}

void OpenGL_VAO::swap(OpenGL_VAO& other) noexcept
{
    std::swap(id, other.id);
    std::swap(vertexSize, other.vertexSize);
    std::swap(attributeCount, other.attributeCount);
}

void OpenGL_VAO::bind() const
{
    glBindVertexArray(id);
}

void OpenGL_VAO::unbind() const
{
    glBindVertexArray(0);
}

void OpenGL_VAO::unbindGlobal()
{
    glBindVertexArray(0);
}

void OpenGL_VAO::setVertexSize(GLsizei vertexSize)
{
    this->vertexSize = vertexSize;
}

void OpenGL_VAO::setAttribute(Type type, GLint componentsCount, bool normalized)
{
    GLuint index = attributeCount++;
    if (type == Type::Float)
    {
        glVertexAttribPointer(index, componentsCount, GL_FLOAT, normalized, vertexSize, (const void*)pointer);
    }
    else if (type == Type::Int)
    {
        glVertexAttribIPointer(index, componentsCount, GL_INT, vertexSize, (const void*)pointer);
    }
    enableAttribute(index);
}

void OpenGL_VAO::enableAttribute(GLuint index)
{
    glEnableVertexAttribArray(index);
}

void OpenGL_VAO::disableAttribute(GLuint index)
{
    glDisableVertexAttribArray(index);
}

void OpenGL_VAO::setAttributeDivisor(GLuint index, GLuint divisor)
{
    glVertexAttribDivisor(index, divisor);
}