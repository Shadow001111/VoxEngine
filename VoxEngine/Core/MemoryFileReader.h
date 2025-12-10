#pragma once
#include <filesystem>
#include <memory>

class MemoryFileReader
{
public:
    enum class Result : uint8_t
    {
        Success,
        FileNotFound,
        FileTooLarge,
        ReadError,
        EmptyFile,
        PositionOutOfBounds,
        InsufficientData
    };
private:
    std::unique_ptr<char[]> m_data;
    size_t m_size;
    size_t m_position;

    // Disable copying and moving
    MemoryFileReader(const MemoryFileReader&) = delete;
    MemoryFileReader& operator=(const MemoryFileReader&) = delete;
    MemoryFileReader(MemoryFileReader&&) = delete;
    MemoryFileReader& operator=(MemoryFileReader&&) = delete;
public:
    MemoryFileReader() noexcept;
    ~MemoryFileReader() noexcept;

    Result loadFile(const std::filesystem::path& filepath, size_t maxSize);

    template<typename T>
    Result read(T* dest, size_t count = 1);

    Result readBytes(void* dest, size_t bytes);
    Result skip(size_t bytes);

    // Getters (inline in header)
    size_t getPosition() const noexcept { return m_position; }
    size_t getSize() const noexcept { return m_size; }
    size_t getRemainingBytes() const noexcept { return m_size - m_position; }
    bool isEndOfFile() const noexcept { return m_position >= m_size; }
    bool isLoaded() const noexcept { return m_data != nullptr; }

    //const char* getData() const noexcept { return m_data.get(); }
    //const char* getCurrentPointer() const noexcept { return m_data ? m_data.get() + m_position : nullptr; }

    // Setters
    Result setPosition(size_t pos);
    void resetToBeginning() noexcept { m_position = 0; }
    void clear() noexcept;
};

template<typename T>
MemoryFileReader::Result MemoryFileReader::read(T* dest, size_t count)
{
    if (!dest)
    {
        return Result::ReadError;
    }

    size_t bytesToRead = sizeof(T) * count;
    if (m_position + bytesToRead > m_size)
    {
        return Result::InsufficientData;
    }

    std::memcpy(dest, m_data.get() + m_position, bytesToRead);
    m_position += bytesToRead;
    return Result::Success;
}