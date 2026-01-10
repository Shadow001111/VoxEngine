#pragma once
#include <glad/glad.h>

class OpenGL_Buffer
{
protected:
	GLenum target = 0;
	GLenum usage = 0;
	GLuint id = 0;
	size_t capacity = 0;
public:
	OpenGL_Buffer() = default;
	~OpenGL_Buffer();

	OpenGL_Buffer(const OpenGL_Buffer& other) = delete;
	OpenGL_Buffer& operator=(const OpenGL_Buffer& other) = delete;

	OpenGL_Buffer(OpenGL_Buffer&& other) noexcept;
	OpenGL_Buffer& operator=(OpenGL_Buffer&& other) noexcept;

	void create(GLenum target, GLenum usage);

	void bind() const;
	void bind(GLenum target) const;

	void unbind() const;
	static void unbind(GLenum target);

	void bindBase(GLuint index) const;
	void bindBase(GLenum target, GLuint index) const;

	void swap(OpenGL_Buffer& other) noexcept;

	void allocateMemory(size_t newSize);

	void write(const void* data, size_t dataSize, size_t offset = 0) const;
	void copyRangeFrom(const OpenGL_Buffer& src, size_t srcOffset, size_t dstOffset, size_t size) const;
	void clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const;

	GLuint getID() const { return id; };
	size_t getCapacity() const { return capacity; };
};

