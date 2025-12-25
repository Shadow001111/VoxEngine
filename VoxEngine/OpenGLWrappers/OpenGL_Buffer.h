#pragma once
#include <glad/glad.h>

class OpenGL_Buffer
{
protected:
	GLenum target;
	GLenum usage;
	GLuint id;
	size_t capacity = 0;
public:
	OpenGL_Buffer(GLenum target, GLenum usage);
	~OpenGL_Buffer();

	OpenGL_Buffer(const OpenGL_Buffer& other) = delete;
	OpenGL_Buffer& operator=(const OpenGL_Buffer& other) = delete;

	OpenGL_Buffer(OpenGL_Buffer&& other) noexcept;
	OpenGL_Buffer& operator=(OpenGL_Buffer&& other) noexcept;

	void swap(OpenGL_Buffer& other) noexcept;

	void bind() const;
	void unbind() const;

	void bindBase(GLuint index) const;

	void allocateMemory(size_t newSize);

	void write(const void* data, size_t dataSize, size_t offset = 0) const;
	void copyRangeFrom(const OpenGL_Buffer& src, size_t srcOffset, size_t dstOffset, size_t size) const;

	GLuint getID() const { return id; };
	size_t getCapacity() const { return capacity; };
};

