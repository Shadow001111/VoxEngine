#include "OpenGL_ImmutableBuffer.h"
#include <iostream>

OpenGL_ImmutableBuffer::~OpenGL_ImmutableBuffer()
{
    if (id)
    {
        glDeleteBuffers(1, &id);
    }
}

OpenGL_ImmutableBuffer::OpenGL_ImmutableBuffer(OpenGL_ImmutableBuffer&& other) noexcept :
    target(other.target),
    id(other.id),
    capacity(other.capacity),
    flags(other.flags)
{
    other.id = 0;
    other.capacity = 0;
    other.flags = 0;
    other.target = 0;
}

OpenGL_ImmutableBuffer& OpenGL_ImmutableBuffer::operator=(OpenGL_ImmutableBuffer&& other) noexcept
{
    if (this != &other)
    {
        if (id)
        {
            glDeleteBuffers(1, &id);
        }

        target = other.target;
        id = other.id;
        capacity = other.capacity;
        flags = other.flags;

        other.id = 0;
        other.capacity = 0;
        other.flags = 0;
        other.target = 0;
    }
    return *this;
}

void OpenGL_ImmutableBuffer::create(GLenum target)
{
    this->target = target;
    if (id) glDeleteBuffers(1, &id);
    glCreateBuffers(1, &id);
}

void OpenGL_ImmutableBuffer::allocateStorage(size_t size, GLbitfield flags, const void* data)
{
    if (id == 0)
    {
        std::cerr << "[OpenGL_ImmutableBuffer][allocateStorage]: Buffer not created! Call create() first.\n";
        return;
    }

    if (capacity > 0)
    {
        std::cerr << "[OpenGL_ImmutableBuffer][allocateStorage]: Storage already allocated! Cannot resize immutable buffer.\n";
        return;
    }

    capacity = size;
    this->flags = flags;
    glNamedBufferStorage(id, capacity, data, flags);
}

void OpenGL_ImmutableBuffer::bind() const
{
    glBindBuffer(target, id);
}

void OpenGL_ImmutableBuffer::bind(GLenum target) const
{
    glBindBuffer(target, id);
}

void OpenGL_ImmutableBuffer::unbind() const
{
    glBindBuffer(target, 0);
}

void OpenGL_ImmutableBuffer::unbind(GLenum target)
{
    glBindBuffer(target, 0);
}

void OpenGL_ImmutableBuffer::bindBase(GLuint index) const
{
    glBindBufferBase(target, index, id);
}

void OpenGL_ImmutableBuffer::bindBase(GLenum target, GLuint index) const
{
    glBindBufferBase(target, index, id);
}

void OpenGL_ImmutableBuffer::swap(OpenGL_ImmutableBuffer& other) noexcept
{
    std::swap(target, other.target);
    std::swap(id, other.id);
    std::swap(capacity, other.capacity);
    std::swap(flags, other.flags);
}

void OpenGL_ImmutableBuffer::write(const void* data, size_t dataSize, size_t offset) const
{
    if (data == nullptr)
    {
        std::cerr << "[OpenGL_ImmutableBuffer][write]: 'data' is nullptr\n";
        return;
    }
    else if (offset + dataSize > capacity)
    {
        std::cerr << "[OpenGL_ImmutableBuffer][write]: Index out of bounds! Attempted write end = "
            << (offset + dataSize) << ", capacity = " << capacity << "\n";
        return;
    }
    else if ((flags & GL_DYNAMIC_STORAGE_BIT) == 0)
    {
        std::cerr << "[OpenGL_ImmutableBuffer][write]: Buffer created without GL_DYNAMIC_STORAGE_BIT. Cannot update after creation.\n";
        return;
    }

    glNamedBufferSubData(id, offset, dataSize, data);
}

// TODO: Add checks for flags
void OpenGL_ImmutableBuffer::copyRangeFrom(const OpenGL_ImmutableBuffer& src, size_t srcOffset, size_t dstOffset, size_t size) const
{
    if (srcOffset + size > src.capacity || dstOffset + size > capacity)
    {
        std::cerr << "[OpenGL_ImmutableBuffer][copyRangeFrom]: Range exceeds buffer capacity\n";
        return;
    }

    glCopyNamedBufferSubData(src.getID(), id,
        static_cast<GLintptr>(srcOffset),
        static_cast<GLintptr>(dstOffset),
        static_cast<GLsizeiptr>(size));
}

void OpenGL_ImmutableBuffer::clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const
{
    glClearNamedBufferData(id, internalFormat, format, type, data);
}