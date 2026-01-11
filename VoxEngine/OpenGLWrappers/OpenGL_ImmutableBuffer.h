#pragma once
#include <glad/glad.h>

class OpenGL_ImmutableBuffer
{
protected:
    GLenum target = 0;
    GLbitfield flags = 0;
    GLuint id = 0;
    size_t capacity = 0;
public:
    OpenGL_ImmutableBuffer() = default;
    ~OpenGL_ImmutableBuffer();

    // Delete copy operations
    OpenGL_ImmutableBuffer(const OpenGL_ImmutableBuffer& other) = delete;
    OpenGL_ImmutableBuffer& operator=(const OpenGL_ImmutableBuffer& other) = delete;

    // Move operations
    OpenGL_ImmutableBuffer(OpenGL_ImmutableBuffer&& other) noexcept;
    OpenGL_ImmutableBuffer& operator=(OpenGL_ImmutableBuffer&& other) noexcept;

    // Create buffer object (without storage)
    void create(GLenum target);

    // Allocate immutable storage
    void allocateStorage(size_t size, GLbitfield flags, const void* data = nullptr);

    // Binding methods
    void bind() const;
    void bind(GLenum target) const;
    void unbind() const;
    static void unbind(GLenum target);

    // Bind to indexed target (for SSBO, UBO, etc.)
    void bindBase(GLuint index) const;
    void bindBase(GLenum target, GLuint index) const;

    // Swap buffers
    void swap(OpenGL_ImmutableBuffer& other) noexcept;

    // Data operations
    void write(const void* data, size_t dataSize, size_t offset = 0) const;
    void copyRangeFrom(const OpenGL_ImmutableBuffer& src, size_t srcOffset, size_t dstOffset, size_t size) const;
    void clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const;

    // Getters
    GLuint getID() const { return id; }
    size_t getCapacity() const { return capacity; }
    GLbitfield getFlags() const { return flags; }
    bool isMappable() const { return (flags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT)) != 0; }
};