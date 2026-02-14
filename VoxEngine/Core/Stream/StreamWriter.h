#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

class StreamWriter
{
public:
    // Constructor opens the file immediately
    explicit StreamWriter(const std::filesystem::path& filepath,
        std::ios::openmode mode = std::ios::binary);

    // Rule of 5 - movable, not copyable
    StreamWriter(const StreamWriter&) = delete;
    StreamWriter& operator=(const StreamWriter&) = delete;
    StreamWriter(StreamWriter&& other) noexcept = default;
    StreamWriter& operator=(StreamWriter&& other) noexcept = default;

    // State checking
    explicit operator bool() const noexcept { return m_stream.good(); }
    bool isOpen() const noexcept { return m_stream.is_open(); }

    // Position management
    size_t tell();
    bool seek(size_t position);
    bool skip(size_t bytes);
    bool rewind() { return seek(0); }

    // Writing methods
    template<typename T>
    bool write(const T* src, size_t count = 1);

    bool writeBytes(const void* src, size_t bytes);
    bool writeString(const std::string& str);
    bool writeString(const char* str);
    bool writeLine(const std::string& line, char delimiter = '\n');

    // Flush to disk
    bool flush();

    // Get the underlying stream (for advanced usage)
    std::ofstream& stream() noexcept { return m_stream; }
    const std::ofstream& stream() const noexcept { return m_stream; }

private:
    std::ofstream m_stream;
};

// Template implementation
template<typename T>
bool StreamWriter::write(const T* src, size_t count)
{
    static_assert(std::is_trivially_copyable_v<T>,
        "Type must be trivially copyable for binary write");

    if (!src || !m_stream)
        return false;

    size_t bytesToWrite = sizeof(T) * count;
    return writeBytes(src, bytesToWrite);
}