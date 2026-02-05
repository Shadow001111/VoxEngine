#pragma once
#include <glad/glad.h>

class ImmutableBuffer
{
    GLenum target = 0;
    GLbitfield flags = 0;
    GLuint id = 0;
    size_t capacity = 0;
public:
    ImmutableBuffer() = default;
    ~ImmutableBuffer();

    ImmutableBuffer(const ImmutableBuffer& other) = delete;
    ImmutableBuffer& operator=(const ImmutableBuffer& other) = delete;

    ImmutableBuffer(ImmutableBuffer&& other) noexcept;
    ImmutableBuffer& operator=(ImmutableBuffer&& other) noexcept;

    void create(GLenum target);
    void destroy();

    void allocateStorage(size_t size, GLbitfield flags, const void* data = nullptr);

    void bind() const;
    void bind(GLenum target) const;
    void unbind() const;
    static void unbind(GLenum target);

    void bindBase(GLuint index) const;
    void bindBase(GLenum target, GLuint index) const;

    void swap(ImmutableBuffer& other) noexcept;

    void write(const void* data, size_t dataSize, size_t offset = 0) const;
    void copyRangeFrom(const ImmutableBuffer& src, size_t srcOffset, size_t dstOffset, size_t size) const;
    void clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const;

    void* map(GLenum access);
    void* mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access);
    void* mapPersistent(GLbitfield access, GLsizeiptr size);

    void unmap();
    void flushMappedRange(GLintptr offset, GLsizeiptr length);

    GLuint getID() const { return id; }
    size_t getCapacity() const { return capacity; }
    GLbitfield getFlags() const { return flags; }
    bool isMappable() const { return (flags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT)) != 0; }
};