#include "VertexArray.h"

VertexArray::~VertexArray()
{
    if (id) glDeleteVertexArrays(1, &id);
}

VertexArray::VertexArray(VertexArray&& other) noexcept
    : id(other.id)
{
    other.id = 0;
}

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept
{
    if (this != &other)
    {
        if (id) glDeleteVertexArrays(1, &id);

        id = other.id;

        other.id = 0;
    }
    return *this;
}

void VertexArray::create()
{
    if (id) glDeleteVertexArrays(1, &id);
    glCreateVertexArrays(1, &id);
}

void VertexArray::destroy()
{
    if (id) glDeleteVertexArrays(1, &id);
}

void VertexArray::bind() const
{
    glBindVertexArray(id);
}

void VertexArray::unbind()
{
    glBindVertexArray(0);
}

void VertexArray::setFloatAttribute(
    GLuint attributeIndex, GLint componentCount, GLsizei stride, GLuint bindingIndex,
    GLenum type, GLboolean normalized)
{
    glVertexArrayAttribFormat(id, attributeIndex, componentCount, type, normalized, stride);
    glVertexArrayAttribBinding(id, attributeIndex, bindingIndex);
}

void VertexArray::setIntAttribute(
    GLuint attributeIndex, GLint componentCount, GLsizei stride, GLuint bindingIndex,
    GLenum type)
{
    glVertexArrayAttribIFormat(id, attributeIndex, componentCount, type, stride);
    glVertexArrayAttribBinding(id, attributeIndex, bindingIndex);
}

void VertexArray::enableAttribute(GLuint index)
{
    glEnableVertexArrayAttrib(id, index);
}

void VertexArray::disableAttribute(GLuint index)
{
    glDisableVertexArrayAttrib(id, index);
}

void VertexArray::setAttributeDivisor(GLuint index, GLuint divisor)
{
    glVertexArrayBindingDivisor(id, index, divisor);
}

void VertexArray::bindVertexBuffer(GLuint bindingIndex, GLuint bufferId, GLintptr offset, GLsizei stride)
{
    glVertexArrayVertexBuffer(id, bindingIndex, bufferId, offset, stride);
}

void VertexArray::bindElementBuffer(GLuint bufferId)
{
    glVertexArrayElementBuffer(id, bufferId);
}
