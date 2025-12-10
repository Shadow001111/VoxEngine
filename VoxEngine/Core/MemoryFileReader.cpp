#include "MemoryFileReader.h"
#include <fstream>
#include <cstring>

MemoryFileReader::MemoryFileReader() noexcept :
    m_data(nullptr), m_size(0), m_position(0) 
{}

MemoryFileReader::~MemoryFileReader() noexcept 
{
    clear();
}

MemoryFileReader::Result MemoryFileReader::loadFile(
    const std::filesystem::path& filepath, size_t maxSize
)
{
    clear();

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return Result::FileNotFound;
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize == 0)
    {
        return Result::EmptyFile;
    }

    if (fileSize < 0)
    {
        return Result::ReadError;
    }

    size_t size = static_cast<size_t>(fileSize);
    if (size > maxSize)
    {
        return Result::FileTooLarge;
    }

    m_size = size;

    m_data = std::make_unique<char[]>(m_size);

    file.seekg(0);
    if (!file.read(m_data.get(), static_cast<std::streamsize>(m_size)))
    {
        clear();
        return Result::ReadError;
    }

    return Result::Success;
}

MemoryFileReader::Result MemoryFileReader::readBytes(void* dest, size_t bytes)
{
    if (!dest)
    {
        return Result::ReadError;
    }

    if (m_position + bytes > m_size)
    {
        return Result::InsufficientData;
    }

    std::memcpy(dest, m_data.get() + m_position, bytes);
    m_position += bytes;
    return Result::Success;
}

MemoryFileReader::Result MemoryFileReader::skip(size_t bytes)
{
    if (m_position + bytes > m_size)
    {
        return Result::PositionOutOfBounds;
    }

    m_position += bytes;
    return Result::Success;
}

MemoryFileReader::Result MemoryFileReader::setPosition(size_t pos)
{
    if (pos > m_size)
    {
        return Result::PositionOutOfBounds;
    }

    m_position = pos;
    return Result::Success;
}

void MemoryFileReader::clear() noexcept
{
    m_data.reset();
    m_size = 0;
    m_position = 0;
}