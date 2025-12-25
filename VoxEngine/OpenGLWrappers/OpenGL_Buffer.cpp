#include "OpenGL_Buffer.h"

#include <iostream>

#include "OpenGLWrappers/openGLDebug.h"

OpenGL_Buffer::OpenGL_Buffer(GLenum target, GLenum usage) :
	target(target), usage(usage)
{
	glGenBuffers(1, &id);
	OPENGL_LOG_BUFFER_CREATED(1, &id);
}

OpenGL_Buffer::~OpenGL_Buffer()
{
	if (id)
	{
		glDeleteBuffers(1, &id);
	}
}

OpenGL_Buffer::OpenGL_Buffer(OpenGL_Buffer&& other) noexcept
	: target(other.target),
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
		if (id)
		{
			glDeleteBuffers(1, &id);
		}

		target = other.target;
		usage = other.usage;
		id = other.id;
		capacity = other.capacity;

		other.id = 0;
		other.capacity = 0;
	}
	return *this;
}

void OpenGL_Buffer::swap(OpenGL_Buffer& other) noexcept
{
	std::swap(target, other.target);
	std::swap(usage, other.usage);
	std::swap(id, other.id);
	std::swap(capacity, other.capacity);
}

void OpenGL_Buffer::bind() const
{
	glBindBuffer(target, id);
}

void OpenGL_Buffer::unbind() const
{
	glBindBuffer(target, 0);
}

void OpenGL_Buffer::bindBase(GLuint index) const
{
	glBindBufferBase(target, index, id);
}

void OpenGL_Buffer::allocateMemory(size_t newSize)
{
	if (newSize > capacity)
	{
		capacity = newSize;
		glBindBuffer(target, id);
		glBufferData(target, capacity, nullptr, usage);
	}
}

void OpenGL_Buffer::write(const void* data, size_t dataSize, size_t offset) const
{
	if (data == nullptr)
	{
		return;
	}
	else if (offset + dataSize > capacity)
	{
		std::cerr << "[OpenGL_Buffer][write]: Index out of bounds! Attempted write end = " << (offset + dataSize) << ", capacity = " << capacity << "\n";
		return;
	}

	OPENGL_CHECK_BIND_TARGET(id, target);
	glBufferSubData(target, offset, dataSize, data);
}

void OpenGL_Buffer::copyRangeFrom(const OpenGL_Buffer& src, size_t srcOffset, size_t dstOffset, size_t size) const
{
	if (srcOffset + size > src.capacity || dstOffset + size > capacity)
	{
		std::cerr << "[OpenGL_Buffer][copyRangeFrom]: Range exceeds buffer capacity\n";
		return;
	}

	glBindBuffer(GL_COPY_READ_BUFFER, src.getID());
	glBindBuffer(GL_COPY_WRITE_BUFFER, id);

	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
		static_cast<GLintptr>(srcOffset),
		static_cast<GLintptr>(dstOffset),
		static_cast<GLsizeiptr>(size));
}
