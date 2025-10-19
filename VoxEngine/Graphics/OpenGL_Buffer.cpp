#include "OpenGL_Buffer.h"

#include <stdexcept>
#include <string>

OpenGL_Buffer::OpenGL_Buffer(GLenum target, GLenum usage) :
	target(target), usage(usage),
	id(0), capacity(0)
{
	glGenBuffers(1, &id);
}

OpenGL_Buffer::~OpenGL_Buffer()
{
	if (id)
	{
		glDeleteBuffers(1, &id);
		id = 0;
	}
}

OpenGL_Buffer::OpenGL_Buffer(OpenGL_Buffer&& other) noexcept
	: target(other.target),
	usage(other.usage),
	id(other.id),
	capacity(other.capacity)
{
	// Leave 'other' in a valid but empty state
	other.id = 0;
	other.capacity = 0;
}

OpenGL_Buffer& OpenGL_Buffer::operator=(OpenGL_Buffer&& other) noexcept
{
	if (this != &other)
	{
		// Delete current buffer if valid
		if (id)
		{
			glDeleteBuffers(1, &id);
		}

		// Move fields
		target = other.target;
		usage = other.usage;
		id = other.id;
		capacity = other.capacity;

		// Reset source
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

	if (offset + dataSize > capacity)
	{
		throw std::runtime_error(
			"OpenGL_Buffer::write: Index out of bounds! "
			"Attempted write end = " + std::to_string(offset + dataSize) +
			", capacity = " + std::to_string(capacity) + "."
		);
	}
	else
	{
		glBufferSubData(target, offset, dataSize, data);
	}
}

void OpenGL_Buffer::copyRangeFrom(const OpenGL_Buffer& src, size_t srcOffset, size_t dstOffset, size_t size) const
{
	if (srcOffset + size > src.capacity || dstOffset + size > capacity)
	{
		throw std::runtime_error("OpenGL_Buffer::copyRangeFrom: Range exceeds buffer capacity.");
	}

	// Bind both buffers to copy targets
	glBindBuffer(GL_COPY_READ_BUFFER, src.getID());
	glBindBuffer(GL_COPY_WRITE_BUFFER, id);

	// Copy data GPU-to-GPU
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
		static_cast<GLintptr>(srcOffset),
		static_cast<GLintptr>(dstOffset),
		static_cast<GLsizeiptr>(size));

	// Optionally unbind (not strictly required)
	/*glBindBuffer(GL_COPY_READ_BUFFER, 0);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);*/
}


GLuint OpenGL_Buffer::getID() const
{
	return id;
}

size_t OpenGL_Buffer::getCapacity() const
{
	return capacity;
}
