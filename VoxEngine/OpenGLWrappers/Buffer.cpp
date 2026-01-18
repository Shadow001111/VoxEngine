#include "Buffer.h"
#include <iostream>

Buffer::~Buffer()
{
	if (id) glDeleteBuffers(1, &id);
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
	if (id) glDeleteBuffers(1, &id);
	glCreateBuffers(1, &id);
}

void Buffer::destroy()
{
	if (id) glDeleteBuffers(1, &id);
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
	std::swap(usage, other.usage);
	std::swap(id, other.id);
	std::swap(capacity, other.capacity);
}

void Buffer::allocateMemory(size_t newSize, const void* data)
{
	if (id == 0)
	{
		std::cerr << "[Buffer][allocateMemory]: Buffer not created! Call create() first.\n";
		return;
	}

	if (newSize > capacity)
	{
		capacity = newSize;
		glNamedBufferData(id, capacity, data, usage);
	}
}

void Buffer::write(const void* data, size_t dataSize, size_t offset) const
{
	if (data == nullptr)
	{
		std::cerr << "[Buffer][write]: 'data' is nullptr\n";
		return;
	}
	else if (offset + dataSize > capacity)
	{
		std::cerr << "[Buffer][write]: Index out of bounds! Attempted write end = " << (offset + dataSize) << ", capacity = " << capacity << "\n";
		return;
	}

	glNamedBufferSubData(id, offset, dataSize, data);
}

void Buffer::copyRangeFrom(const Buffer& src, size_t srcOffset, size_t dstOffset, size_t size) const
{
	if (srcOffset + size > src.capacity || dstOffset + size > capacity)
	{
		std::cerr << "[Buffer][copyRangeFrom]: Range exceeds buffer capacity\n";
		return;
	}

	glCopyNamedBufferSubData(src.getID(), id,
		static_cast<GLintptr>(srcOffset),
		static_cast<GLintptr>(dstOffset),
		static_cast<GLsizeiptr>(size));
}

void Buffer::clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const
{
	glClearNamedBufferData(id, internalFormat, format, type, data);
}
