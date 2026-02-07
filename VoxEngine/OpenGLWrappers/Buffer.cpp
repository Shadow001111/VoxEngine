#include "Buffer.h"
#include <iostream>

#define BUFFER_SAFETY_CHECKS 1

Buffer::~Buffer()
{
	destroy();
}

Buffer::Buffer(Buffer&& other) noexcept :
	target(other.target),
	usage(other.usage),
	id(other.id),
	capacity(other.capacity)
{
	other.id = 0;
	other.capacity = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
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

void Buffer::create(GLenum target, GLenum usage)
{
	this->target = target;
	this->usage = usage;
	this->capacity = 0;

	if (id)
	{
		glDeleteBuffers(1, &id);
	}
	glCreateBuffers(1, &id);
}

void Buffer::destroy()
{
	this->target = target;
	this->usage = usage;
	this->capacity = 0;

	if (id)
	{
		glDeleteBuffers(1, &id);
		id = 0;
	}
}

void Buffer::bind() const
{
	glBindBuffer(this->target, id);
}

void Buffer::bind(GLenum target) const
{
	glBindBuffer(target, id);
}

void Buffer::unbind() const
{
	glBindBuffer(this->target, 0);
}

void Buffer::unbind(GLenum target)
{
	glBindBuffer(target, 0);
}

void Buffer::bindBase(GLuint index) const
{
	glBindBufferBase(this->target, index, id);
}

void Buffer::bindBase(GLenum target, GLuint index) const
{
	glBindBufferBase(target, index, id);
}

void Buffer::swap(Buffer& other) noexcept
{
	std::swap(target, other.target);
	std::swap(usage, other.usage);
	std::swap(id, other.id);
	std::swap(capacity, other.capacity);
}

void Buffer::allocateMemoryIfNeeded(size_t newSize, const void* data)
{
#if BUFFER_SAFETY_CHECKS
	if (id == 0)
	{
		std::cerr << "[Buffer][allocateMemoryIfNeeded]: Buffer not created! Call create() first.\n";
		return;
	}
	if (newSize == 0)
	{
		std::cerr << "[Buffer][allocateMemoryIfNeeded]: newSize must be greater than 0.\n";
		return;
	}
#endif
	if (newSize > capacity)
	{
		capacity = newSize;
		glNamedBufferData(id, capacity, data, usage);
	}
}

void Buffer::write(const void* data, size_t dataSize, size_t offset) const
{
#if BUFFER_SAFETY_CHECKS
	if (id == 0)
	{
		std::cerr << "[Buffer][write]: Buffer not created! Call create() first.\n";
		return;
	}
	if (data == nullptr)
	{
		std::cerr << "[Buffer][write]: 'data' is nullptr.\n";
		return;
	}
	if (dataSize == 0)
	{
		std::cerr << "[Buffer][write]: Size must be greater than 0.\n";
		return;
	}
	if (offset + dataSize > capacity)
	{
		std::cerr << "[Buffer][write]: Index out of bounds! Start: " << offset << ", Size: " << dataSize << ", Capacity: " << capacity << ".\n";
		return;
	}
#endif
	glNamedBufferSubData(id, offset, dataSize, data);
}

void Buffer::copyRangeFrom(const Buffer& src, size_t srcOffset, size_t dstOffset, size_t size) const
{
#if BUFFER_SAFETY_CHECKS
	if (id == 0)
	{
		std::cerr << "[Buffer][copyRangeFrom]: Destination buffer not created! Call create() first.\n";
		return;
	}
	if (src.getID() == 0)
	{
		std::cerr << "[Buffer][copyRangeFrom]: Source buffer not created! Call create() first.\n";
		return;
	}
	if (size == 0)
	{
		std::cerr << "[Buffer][copyRangeFrom]: Size must be greater than 0.\n";
		return;
	}
	if (srcOffset + size > src.capacity || dstOffset + size > capacity)
	{
		std::cerr << "[Buffer][copyRangeFrom]: Range exceeds buffer capacity\n";
		return;
	}
#endif
	glCopyNamedBufferSubData(src.getID(), id,
		static_cast<GLintptr>(srcOffset),
		static_cast<GLintptr>(dstOffset),
		static_cast<GLsizeiptr>(size));
}

void Buffer::clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const
{
	glClearNamedBufferData(id, internalFormat, format, type, data);
}

void* Buffer::map(GLenum access)
{
#if BUFFER_SAFETY_CHECKS
	if (id == 0)
	{
		std::cerr << "[Buffer][map]: Buffer not created! Call create() first.\n";
		return nullptr;
	}
	if (capacity == 0)
	{
		std::cerr << "[Buffer][map]: Buffer has zero capacity! Cannot map.\n";
		return nullptr;
	}
#endif
	void* ptr = glMapNamedBuffer(id, access);
#if BUFFER_SAFETY_CHECKS
	if (ptr == nullptr)
	{
		std::cerr << "[Buffer][map]: Failed to map buffer.\n";
	}
#endif
	return ptr;
}

void* Buffer::mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access)
{
#if BUFFER_SAFETY_CHECKS
	if (id == 0)
	{
		std::cerr << "[Buffer][mapRange]: Buffer not created! Call create() first.\n";
		return nullptr;
	}
	if (length <= 0)
	{
		std::cerr << "[Buffer][mapRange]: Length must be greater than 0.\n";
		return nullptr;
	}
	if (offset + length > capacity)
	{
		std::cerr << "[Buffer][mapRange]: Index out of bounds! Start: " << offset << ", Size: " << length << ", Capacity: " << capacity << ".\n";
		return nullptr;
	}
#endif
	void* ptr = glMapNamedBufferRange(id, offset, length, access);
#if BUFFER_SAFETY_CHECKS
	if (!ptr)
	{
		std::cerr << "[Buffer][mapRange]: Failed to map buffer.\n";
	}
#endif
	return ptr;
}

void Buffer::unmap()
{
#if BUFFER_SAFETY_CHECKS
	if (id == 0)
	{
		std::cerr << "[Buffer][unmap]: Buffer not created! Call create() first.\n";
		return;
	}
#endif
	GLboolean result = glUnmapNamedBuffer(id);
#if BUFFER_SAFETY_CHECKS
	if (result == GL_FALSE)
	{
		std::cerr << "[Buffer][unmap]: Failed to unmap buffer.\n";
	}
#endif
}

void Buffer::flushMappedRange(GLintptr offset, GLsizeiptr length)
{
#if BUFFER_SAFETY_CHECKS
	if (id == 0)
	{
		std::cerr << "[Buffer][flushMappedRange]: Buffer not created! Call create() first.\n";
		return;
	}
	if (length <= 0)
	{
		std::cerr << "[Buffer][flushMappedRange]: Length must be greater than 0.\n";
		return;
	}
	if (offset + length > capacity)
	{
		std::cerr << "[Buffer][flushMappedRange]: Index out of bounds! Start: " << offset << ", Size: " << length << ", Capacity: " << capacity << ".\n";
		return;
	}
#endif
	glFlushMappedNamedBufferRange(id, offset, length);
}
