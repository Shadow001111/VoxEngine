#include "StreamWriter.h"

StreamWriter::StreamWriter(const std::filesystem::path& filepath, std::ios::openmode mode)
    : m_stream(filepath, mode | std::ios::binary)  // Always add binary flag
{}

size_t StreamWriter::tell()
{
    if (!m_stream)
        return 0;

    auto pos = m_stream.tellp();
    if (pos < 0)
        return 0;

    return static_cast<size_t>(pos);
}

bool StreamWriter::seek(size_t position)
{
    if (!m_stream)
        return false;

    m_stream.seekp(static_cast<std::streamoff>(position), std::ios::beg);
    return m_stream.good();
}

bool StreamWriter::skip(size_t bytes)
{
    if (!m_stream)
        return false;

    m_stream.seekp(static_cast<std::streamoff>(bytes), std::ios::cur);
    return m_stream.good();
}

bool StreamWriter::writeBytes(const void* src, size_t bytes)
{
    if (!src || !m_stream || bytes == 0)
        return false;

    m_stream.write(static_cast<const char*>(src), static_cast<std::streamsize>(bytes));
    return m_stream.good();
}

bool StreamWriter::writeString(const std::string& str)
{
    return writeBytes(str.data(), str.size());
}

bool StreamWriter::writeString(const char* str)
{
    if (!str)
        return false;
    return writeBytes(str, std::strlen(str));
}

bool StreamWriter::writeLine(const std::string& line, char delimiter)
{
    if (!m_stream)
        return false;

    m_stream << line << delimiter;
    return m_stream.good();
}

bool StreamWriter::flush()
{
    if (!m_stream)
        return false;

    m_stream.flush();
    return m_stream.good();
}