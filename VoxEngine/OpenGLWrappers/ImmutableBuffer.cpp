#include "ImmutableBuffer.h"
#include <iostream>

#define BUFFER_SAFETY_CHECKS 1

ImmutableBuffer::~ImmutableBuffer()
{
    destroy();
}

ImmutableBuffer::ImmutableBuffer(ImmutableBuffer&& other) noexcept :
    target(other.target),
    id(other.id),
    capacity(other.capacity),
    flags(other.flags),
	persistentMappedPtr(other.persistentMappedPtr)
{
    other.id = 0;
    other.capacity = 0;
    other.flags = 0;
    other.target = 0;
	other.persistentMappedPtr = nullptr;
}

ImmutableBuffer& ImmutableBuffer::operator=(ImmutableBuffer&& other) noexcept
{
    if (this != &other)
    {
        if (id) glDeleteBuffers(1, &id);

        target = other.target;
        id = other.id;
        capacity = other.capacity;
        flags = other.flags;
		persistentMappedPtr = other.persistentMappedPtr;

        other.id = 0;
        other.capacity = 0;
        other.flags = 0;
        other.target = 0;
		other.persistentMappedPtr = nullptr;
    }
    return *this;
}

void ImmutableBuffer::create(GLenum target)
{
    this->target = target;
    if (id)
    {
        glDeleteBuffers(1, &id);
        persistentMappedPtr = nullptr;
    }
    glCreateBuffers(1, &id);
}

void ImmutableBuffer::destroy()
{
    if (id)
    {
        glDeleteBuffers(1, &id);
        id = 0;
        persistentMappedPtr = nullptr;
    }
}

void ImmutableBuffer::allocateStorage(size_t size, GLbitfield flags, const void* data)
{
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][allocateStorage]: Buffer not created! Call create() first.\n";
        return;
	}
    if (size == 0)
    {
        std::cerr << "[ImmutableBuffer][allocateStorage]: Size must be greater than 0.\n";
        return;
	}
    if (capacity > 0)
    {
        std::cerr << "[ImmutableBuffer][allocateStorage]: Storage already allocated! Cannot resize immutable buffer.\n";
        return;
    }
#endif
    capacity = size;
    this->flags = flags;
    glNamedBufferStorage(id, capacity, data, flags);
}

void ImmutableBuffer::bind() const
{
    glBindBuffer(target, id);
}

void ImmutableBuffer::bind(GLenum target) const
{
    glBindBuffer(target, id);
}

void ImmutableBuffer::unbind() const
{
    glBindBuffer(target, 0);
}

void ImmutableBuffer::unbind(GLenum target)
{
    glBindBuffer(target, 0);
}

void ImmutableBuffer::bindBase(GLuint index) const
{
    glBindBufferBase(target, index, id);
}

void ImmutableBuffer::bindBase(GLenum target, GLuint index) const
{
    glBindBufferBase(target, index, id);
}

void ImmutableBuffer::swap(ImmutableBuffer& other) noexcept
{
    std::swap(target, other.target);
    std::swap(id, other.id);
    std::swap(capacity, other.capacity);
    std::swap(flags, other.flags);
}

void ImmutableBuffer::write(const void* data, size_t dataSize, size_t offset) const
{
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][write]: Buffer not created! Call create() first.\n";
        return;
    }
    if (data == nullptr)
    {
        std::cerr << "[ImmutableBuffer][write]: 'data' is nullptr.\n";
        return;
    }
    if (dataSize == 0)
    {
        std::cerr << "[ImmutableBuffer][write]: Size must be greater than 0.\n";
        return;
	}
    if (offset + dataSize > capacity)
    {
		std::cerr << "[ImmutableBuffer][write]: Index out of bounds! Start: " << offset << ", Size: " << dataSize << ", Capacity: " << capacity << ".\n";
        return;
    }
#endif
    glNamedBufferSubData(id, offset, dataSize, data);
}

void ImmutableBuffer::writePersistentMapped(const void* data, size_t dataSize, size_t offset) const
{
    if (!persistentMappedPtr)
    {
#if BUFFER_SAFETY_CHECKS
		std::cerr << "[ImmutableBuffer][writePersistentMapped]: Buffer is not persistently mapped! Call mapPersistent() first.\n";
#endif
		return;
    }
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][writePersistentMapped]: Buffer not created! Call create() first.\n";
        return;
    }
    if (data == nullptr)
    {
        std::cerr << "[ImmutableBuffer][writePersistentMapped]: 'data' is nullptr.\n";
        return;
    }
    if (dataSize == 0)
    {
        std::cerr << "[ImmutableBuffer][writePersistentMapped]: Size must be greater than 0.\n";
        return;
    }
    if (offset + dataSize > capacity)
    {
        std::cerr << "[ImmutableBuffer][writePersistentMapped]: Index out of bounds! Start: " << offset << ", Size: " << dataSize << ", Capacity: " << capacity << ".\n";
        return;
    }
#endif
    std::memcpy(static_cast<char*>(persistentMappedPtr) + offset, data, dataSize);
}

void ImmutableBuffer::writePersistentMappedWithFallback(const void* data, size_t dataSize, size_t offset) const
{
    if (!persistentMappedPtr)
    {
		// Fallback to regular write if not persistently mapped
        write(data, dataSize, offset);
		return;
    }
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][writePersistentMapped]: Buffer not created! Call create() first.\n";
        return;
    }
    if (data == nullptr)
    {
        std::cerr << "[ImmutableBuffer][writePersistentMapped]: 'data' is nullptr.\n";
        return;
    }
    if (dataSize == 0)
    {
        std::cerr << "[ImmutableBuffer][writePersistentMapped]: Size must be greater than 0.\n";
        return;
	}
    if (offset + dataSize > capacity)
    {
        std::cerr << "[ImmutableBuffer][writePersistentMapped]: Index out of bounds! Start: " << offset << ", Size: " << dataSize << ", Capacity: " << capacity << ".\n";
        return;
    }
#endif
	std::memcpy(static_cast<char*>(persistentMappedPtr) + offset, data, dataSize);
}

// TODO: Add more checks for flags
void ImmutableBuffer::copyRangeFrom(const ImmutableBuffer& src, size_t srcOffset, size_t dstOffset, size_t size) const
{
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][copyRangeFrom]: Destination buffer not created! Call create() first.\n";
        return;
    }
    if (src.getID() == 0)
    {
        std::cerr << "[ImmutableBuffer][copyRangeFrom]: Source buffer not created! Call create() first.\n";
        return;
    }
    if (size == 0)
    {
        std::cerr << "[ImmutableBuffer][copyRangeFrom]: Size must be greater than 0.\n";
		return;
    }
    if (srcOffset + size > src.capacity || dstOffset + size > capacity)
    {
        std::cerr << "[ImmutableBuffer][copyRangeFrom]: Range exceeds buffer capacity.\n";
        return;
    }
#endif
    glCopyNamedBufferSubData(src.getID(), id,
        static_cast<GLintptr>(srcOffset),
        static_cast<GLintptr>(dstOffset),
        static_cast<GLsizeiptr>(size));
}

void ImmutableBuffer::clearData(GLenum internalFormat, GLenum format, GLenum type, const void* data) const
{
    glClearNamedBufferData(id, internalFormat, format, type, data);
}

void* ImmutableBuffer::map(GLenum access)
{
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][map]: Buffer not created! Call create() first.\n";
        return nullptr;
    }
    if (persistentMappedPtr)
    {
        std::cerr << "[ImmutableBuffer][map]: Buffer is already persistently mapped! Unmap first before mapping again.\n";
        return nullptr;
	}
    if (!isMappable())
    {
        std::cerr << "[ImmutableBuffer][mapPersistent]: Buffer was not created with mappable flags! Cannot map persistently.\n";
        return nullptr;
    }
#endif
    void* ptr = glMapNamedBuffer(id, access);
#if BUFFER_SAFETY_CHECKS
    if (ptr == nullptr)
    {
        std::cerr << "[ImmutableBuffer][ImmutableBuffer]: Failed to map buffer.\n";
    }
#endif
    return ptr;
}

void* ImmutableBuffer::mapRange(GLintptr offset, GLsizeiptr size, GLbitfield access)
{
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][mapRange]: Buffer not created! Call create() first.\n";
        return nullptr;
    }
    if (persistentMappedPtr)
    {
        std::cerr << "[ImmutableBuffer][mapRange]: Buffer is already persistently mapped! Unmap first before mapping again.\n";
        return nullptr;
    }
    if (size <= 0)
    {
        std::cerr << "[ImmutableBuffer][mapRange]: Size must be greater than 0.\n";
        return nullptr;
    }
    if (offset + size > capacity)
    {
        std::cerr << "[ImmutableBuffer][mapRange]: Index out of bounds! Start: " << offset << ", Size: " << size << ", Capacity: " << capacity << ".\n";
        return nullptr;
    }
    if (!isMappable())
    {
        std::cerr << "[ImmutableBuffer][mapPersistent]: Buffer was not created with mappable flags! Cannot map persistently.\n";
        return nullptr;
    }
#endif
    void* ptr = glMapNamedBufferRange(id, offset, size, access);
#if BUFFER_SAFETY_CHECKS
    if (!ptr)
    {
        std::cerr << "[ImmutableBuffer][mapRange]: Failed to map buffer.\n";
    }
#endif
    return ptr;
}

void* ImmutableBuffer::mapPersistent(GLbitfield access, GLsizeiptr size)
{
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][mapPersistent]: Buffer not created! Call create() first.\n";
        return nullptr;
    }
    if (size <= 0)
    {
        std::cerr << "[ImmutableBuffer][mapPersistent]: Size must be greater than 0.\n";
        return nullptr;
	}
    if (size > capacity)
    {
        std::cerr << "[ImmutableBuffer][mapPersistent]: Size exceeds buffer capacity! Size: " << size << ", Capacity: " << capacity << ".\n";
        return nullptr;
    }
    if (persistentMappedPtr)
    {
        std::cerr << "[ImmutableBuffer][mapPersistent]: Buffer is already persistently mapped! Unmap first before mapping again.\n";
        return nullptr;
	}
    if (!isMappable())
    {
		std::cerr << "[ImmutableBuffer][mapPersistent]: Buffer was not created with mappable flags! Cannot map persistently.\n";
        return nullptr;
    }
#endif
    persistentMappedPtr = glMapNamedBufferRange(id, 0, size, access | GL_MAP_PERSISTENT_BIT);
    if (!persistentMappedPtr)
    {
#if BUFFER_SAFETY_CHECKS
        std::cerr << "[ImmutableBuffer][mapPersistent]: Failed to map buffer.\n";
#endif
    }
    return persistentMappedPtr;
}

void* ImmutableBuffer::mapPersistent(GLbitfield access)
{
	return mapPersistent(access, capacity);
}

void ImmutableBuffer::unmap()
{
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][unmap]: Buffer not created! Call create() first.\n";
        return;
    }
#endif
    GLboolean result = glUnmapNamedBuffer(id);
#if BUFFER_SAFETY_CHECKS
    if (result == GL_FALSE)
    {
        std::cerr << "[ImmutableBuffer][unmap]: Failed to unmap buffer.\n";
    }
#endif
}

void ImmutableBuffer::flushMappedRange(GLintptr offset, GLsizeiptr size)
{
#if BUFFER_SAFETY_CHECKS
    if (id == 0)
    {
        std::cerr << "[ImmutableBuffer][flushMappedRange]: Buffer not created! Call create() first.\n";
        return;
    }
    if (size <= 0)
    {
        std::cerr << "[ImmutableBuffer][flushMappedRange]: Size must be greater than 0.\n";
		return;
    }
    if (offset + size > capacity)
    {
        std::cerr << "[ImmutableBuffer][flushMappedRange]: Index out of bounds! Start: " << offset << ", Size: " << size << ", Capacity: " << capacity << ".\n";
        return;
    }
#endif
    glFlushMappedNamedBufferRange(id, offset, size);
}