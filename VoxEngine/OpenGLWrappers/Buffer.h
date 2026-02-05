#pragma once
#include <glad/glad.h>

class Buffer
{
	GLenum target = 0;
	GLenum usage = 0;
	GLuint id = 0;
	size_t capacity = 0;
public:
	Buffer() = default;
	~Buffer();

	Buffer(const Buffer& other) = delete;
	Buffer& operator=(const Buffer& other) = delete;

	Buffer(Buffer&& other) noexcept;
	Buffer& operator=(Buffer&& other) noexcept;

	void create(GLenum target, GLenum usage);
	void destroy();

	void bind() const;
	void bind(GLenum target) const;

	void unbind() const;
	static void unbind(GLenum target);

	void bindBase(GLuint index) const;
	void bindBase(GLenum target, GLuint index) const;

	void swap(Buffer& other) noexcept;

	void allocateMemoryIfNeeded(size_t newSize, const void* data = nullptr);

	void write(const void* data, size_t dataSize, size_t offset = 0) const;
	void copyRangeFrom(const Buffer& src, size_t srcOffset, size_t dstOffset, size_t size) const;
	void clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const;

	void* map(GLenum access);
	void* mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access);

	void unmap();
	void flushMappedRange(GLintptr offset, GLsizeiptr length);

	GLuint getID() const { return id; };
	size_t getCapacity() const { return capacity; };
};

