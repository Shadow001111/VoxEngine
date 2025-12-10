#include "MemoryFileWriter.h"
#include <fstream>
#include <cstring>
#include <string>

MemoryFileWriter::MemoryFileWriter() noexcept :
    m_buffer(nullptr),
    m_capacity(0),
    m_position(0),
    m_committed_size(0)
{}

MemoryFileWriter::~MemoryFileWriter() noexcept 
{
    reset();
}

MemoryFileWriter::Result MemoryFileWriter::initialize(size_t capacity) {
    reset();

    if (capacity == 0) 
    {
        return Result::InsufficientCapacity;
    }

    m_buffer = std::make_unique<char[]>(capacity);
    m_capacity = capacity;
    m_position = 0;
    m_committed_size = 0;
    return Result::Success;
}

MemoryFileWriter::Result MemoryFileWriter::writeBytes(const void* src, size_t bytes)
{
    if (!src)
    {
        return Result::WriteError;
    }

    if (!m_buffer)
    {
        return Result::NotInitialized;
    }

    if (m_position + bytes > m_capacity)
    {
        return Result::InsufficientCapacity;
    }

    std::memcpy(m_buffer.get() + m_position, src, bytes);
    m_position += bytes;

    if (m_position > m_committed_size)
    {
        m_committed_size = m_position;
    }

    return Result::Success;
}

MemoryFileWriter::Result MemoryFileWriter::writeString(const char* str)
{
    if (!str)
    {
        return Result::WriteError;
    }

    size_t length = std::strlen(str);
    return writeBytes(str, length);
}

MemoryFileWriter::Result MemoryFileWriter::writeString(const std::string& str) {
    return writeBytes(str.data(), str.size());
}

MemoryFileWriter::Result MemoryFileWriter::saveToFile(const std::filesystem::path& filepath)
{
    if (!m_buffer)
    {
        return Result::NotInitialized;
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file)
    {
        return Result::FileCreateError;
    }

    if (!file.write(m_buffer.get(), static_cast<std::streamsize>(m_committed_size)))
    {
        return Result::WriteError;
    }

    return Result::Success;
}

MemoryFileWriter::Result MemoryFileWriter::setPosition(size_t pos)
{
    if (!m_buffer)
    {
        return Result::NotInitialized;
    }

    if (pos > m_capacity)
    {
        return Result::PositionOutOfBounds;
    }

    m_position = pos;
    return Result::Success;
}

MemoryFileWriter::Result MemoryFileWriter::skip(size_t bytes) {
    if (!m_buffer)
    {
        return Result::NotInitialized;
    }

    size_t newPosition = m_position + bytes;
    if (newPosition > m_capacity)
    {
        return Result::PositionOutOfBounds;
    }

    m_position = newPosition;
    return Result::Success;
}

void MemoryFileWriter::reset() noexcept
{
    m_buffer.reset();
    m_capacity = 0;
    m_position = 0;
}