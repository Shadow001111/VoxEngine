#include "OpenGL_Buffer.h"
#include <iostream>

OpenGL_Buffer::~OpenGL_Buffer()
{
	if (id) glDeleteBuffers(1, &id);
}

OpenGL_Buffer::OpenGL_Buffer(OpenGL_Buffer&& other) noexcept :
	target(other.target),
	usage(other.usage),
	id(other.id),
	capacity(other.capacity)
{
	other.id = 0;
	other.capacity = 0;
}

OpenGL_Buffer& OpenGL_Buffer::operator=(OpenGL_Buffer&& other) noexcept
{
	if (this != &other)
	{
		if (id) glDeleteBuffers(1, &id);

		target = other.target;
		usage = other.usage;
		id = other.id;
		capacity = other.capacity;

		other.id = 0;
		other.capacity = 0;
	}
	return *this;
}

void OpenGL_Buffer::create(GLenum target, GLenum usage)
{
	this->target = target;
	this->usage = usage;
	if (id) glDeleteBuffers(1, &id);
	glCreateBuffers(1, &id);
}

void OpenGL_Buffer::destroy()
{
	if (id) glDeleteBuffers(1, &id);
}

void OpenGL_Buffer::bind() const
{
	glBindBuffer(this->target, id);
}

void OpenGL_Buffer::bind(GLenum target) const
{
	glBindBuffer(target, id);
}

void OpenGL_Buffer::unbind() const
{
	glBindBuffer(this->target, 0);
}

void OpenGL_Buffer::unbind(GLenum target)
{
	glBindBuffer(target, 0);
}

void OpenGL_Buffer::bindBase(GLuint index) const
{
	glBindBufferBase(this->target, index, id);
}

void OpenGL_Buffer::bindBase(GLenum target, GLuint index) const
{
	glBindBufferBase(target, index, id);
}

void OpenGL_Buffer::swap(OpenGL_Buffer& other) noexcept
{
	std::swap(usage, other.usage);
	std::swap(id, other.id);
	std::swap(capacity, other.capacity);
}

void OpenGL_Buffer::allocateMemory(size_t newSize, const void* data)
{
	if (id == 0)
	{
		std::cerr << "[OpenGL_Buffer][allocateMemory]: Buffer not created! Call create() first.\n";
		return;
	}

	if (newSize > capacity)
	{
		capacity = newSize;
		glNamedBufferData(id, capacity, data, usage);
	}
}

void OpenGL_Buffer::write(const void* data, size_t dataSize, size_t offset) const
{
	if (data == nullptr)
	{
		std::cerr << "[OpenGL_Buffer][write]: 'data' is nullptr\n";
		return;
	}
	else if (offset + dataSize > capacity)
	{
		std::cerr << "[OpenGL_Buffer][write]: Index out of bounds! Attempted write end = " << (offset + dataSize) << ", capacity = " << capacity << "\n";
		return;
	}

	glNamedBufferSubData(id, offset, dataSize, data);
}

void OpenGL_Buffer::copyRangeFrom(const OpenGL_Buffer& src, size_t srcOffset, size_t dstOffset, size_t size) const
{
	if (srcOffset + size > src.capacity || dstOffset + size > capacity)
	{
		std::cerr << "[OpenGL_Buffer][copyRangeFrom]: Range exceeds buffer capacity\n";
		return;
	}

	glCopyNamedBufferSubData(src.getID(), id,
		static_cast<GLintptr>(srcOffset),
		static_cast<GLintptr>(dstOffset),
		static_cast<GLsizeiptr>(size));
}

void OpenGL_Buffer::clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const
{
	glClearNamedBufferData(id, internalFormat, format, type, data);
}
