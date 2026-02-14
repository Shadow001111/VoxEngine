#pragma once
#include <filesystem>
#include <fstream>
#include <type_traits>

class StreamReader
{
    std::ifstream m_stream;
public:
    // Constructor opens the file immediately
    explicit StreamReader(const std::filesystem::path& filepath);

    // Rule of 5 - movable, not copyable
    StreamReader(const StreamReader&) = delete;
    StreamReader& operator=(const StreamReader&) = delete;
    StreamReader(StreamReader&& other) noexcept = default;
    StreamReader& operator=(StreamReader&& other) noexcept = default;

    // State checking
    explicit operator bool() const noexcept { return m_stream.good(); }
    bool isOpen() const noexcept { return m_stream.is_open(); }
    bool isEOF() const noexcept { return m_stream.eof(); }

    // Position management
    size_t tell();
    bool seek(size_t position);
    bool skip(size_t bytes);
    bool rewind() { return seek(0); }

    // Reading methods
    template<typename T>
    bool read(T* dest, size_t count = 1);

    bool readBytes(void* dest, size_t bytes);

    // String reading helpers
    bool readString(std::string& out, size_t maxLength = 1024);
    bool readLine(std::string& out, char delimiter = '\n');

    // Get the underlying stream (for advanced usage)
    std::ifstream& stream() noexcept { return m_stream; }
    const std::ifstream& stream() const noexcept { return m_stream; }

    // Clear error state
    void clear() { m_stream.clear(); }
};

// Template implementation
template<typename T>
bool StreamReader::read(T* dest, size_t count)
{
    static_assert(std::is_trivially_copyable_v<T>,
        "Type must be trivially copyable for binary read");

    if (!dest || !m_stream)
        return false;

    size_t bytesToRead = sizeof(T) * count;
    return readBytes(dest, bytesToRead);
}