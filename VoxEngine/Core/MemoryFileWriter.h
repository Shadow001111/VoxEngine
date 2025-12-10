#pragma once
#include <filesystem>
#include <memory>

class MemoryFileWriter
{
public:
    enum class Result
    {
        Success,
        FileCreateError,
        WriteError,
        InsufficientCapacity,
        PositionOutOfBounds,
        NotInitialized
    };
private:
    std::unique_ptr<char[]> m_buffer;
    size_t m_capacity;
    size_t m_position;
    size_t m_committed_size;

    // Disable copying and moving
    MemoryFileWriter(const MemoryFileWriter&) = delete;
    MemoryFileWriter& operator=(const MemoryFileWriter&) = delete;
    MemoryFileWriter(MemoryFileWriter&&) = delete;
    MemoryFileWriter& operator=(MemoryFileWriter&&) = delete;
public:
    MemoryFileWriter() noexcept;
    ~MemoryFileWriter() noexcept;

    Result initialize(size_t capacity);

    template<typename T>
    Result write(const T* src, size_t count = 1);

    Result writeBytes(const void* src, size_t bytes);
    Result writeString(const char* str);
    Result writeString(const std::string& str);

    Result saveToFile(const std::filesystem::path& filepath);

    Result setPosition(size_t pos);
    Result skip(size_t bytes);
    void resetToBeginning() noexcept { m_position = 0; }
    void reset() noexcept;

    // Getters (inline in header)
    size_t getPosition() const noexcept { return m_position; }
    size_t getCapacity() const noexcept { return m_capacity; }
    size_t getRemainingCapacity() const noexcept { return m_capacity - m_position; }
    bool isInitialized() const noexcept { return m_buffer != nullptr; }

    const char* getData() const noexcept { return m_buffer.get(); }
    char* getData() noexcept { return m_buffer.get(); }

    //const char* getCurrentPointer() const noexcept
    //{
    //    return m_buffer ? m_buffer.get() + m_position : nullptr;
    //}
    //char* getCurrentPointer() noexcept
    //{
    //    return m_buffer ? m_buffer.get() + m_position : nullptr;
    //}
};

// Template implementation must be in header
template<typename T>
MemoryFileWriter::Result MemoryFileWriter::write(const T* src, size_t count)
{
    if (!src)
    {
        return Result::WriteError;
    }

    size_t bytesToWrite = sizeof(T) * count;
    if (m_position + bytesToWrite > m_capacity)
    {
        return Result::InsufficientCapacity;
    }

    std::memcpy(m_buffer.get() + m_position, src, bytesToWrite);
    m_position += bytesToWrite;

    // Update committed size if we wrote past it
    if (m_position > m_committed_size)
    {
        m_committed_size = m_position;
    }

    return Result::Success;
}