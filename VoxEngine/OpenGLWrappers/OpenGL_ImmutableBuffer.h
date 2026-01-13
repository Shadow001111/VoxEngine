#pragma once
#include <glad/glad.h>

class OpenGL_ImmutableBuffer
{
    GLenum target = 0;
    GLbitfield flags = 0;
    GLuint id = 0;
    size_t capacity = 0;
public:
    OpenGL_ImmutableBuffer() = default;
    ~OpenGL_ImmutableBuffer();

    OpenGL_ImmutableBuffer(const OpenGL_ImmutableBuffer& other) = delete;
    OpenGL_ImmutableBuffer& operator=(const OpenGL_ImmutableBuffer& other) = delete;

    OpenGL_ImmutableBuffer(OpenGL_ImmutableBuffer&& other) noexcept;
    OpenGL_ImmutableBuffer& operator=(OpenGL_ImmutableBuffer&& other) noexcept;

    void create(GLenum target);
    void destroy();

    void allocateStorage(size_t size, GLbitfield flags, const void* data = nullptr);

    void bind() const;
    void bind(GLenum target) const;
    void unbind() const;
    static void unbind(GLenum target);

    void bindBase(GLuint index) const;
    void bindBase(GLenum target, GLuint index) const;

    void swap(OpenGL_ImmutableBuffer& other) noexcept;

    void write(const void* data, size_t dataSize, size_t offset = 0) const;
    void copyRangeFrom(const OpenGL_ImmutableBuffer& src, size_t srcOffset, size_t dstOffset, size_t size) const;
    void clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const;

    GLuint getID() const { return id; }
    size_t getCapacity() const { return capacity; }
    GLbitfield getFlags() const { return flags; }
    bool isMappable() const { return (flags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT)) != 0; }
};