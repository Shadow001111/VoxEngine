#include "StreamReader.h"
#include <limits>

StreamReader::StreamReader(const std::filesystem::path& filepath)
    : m_stream(filepath, std::ios::binary)
{}

size_t StreamReader::tell()
{
    if (!m_stream)
        return 0;

    auto pos = m_stream.tellg();
    if (pos < 0)
        return 0;

    return static_cast<size_t>(pos);
}

bool StreamReader::seek(size_t position)
{
    if (!m_stream)
        return false;

    m_stream.seekg(static_cast<std::streamoff>(position), std::ios::beg);
    return m_stream.good();
}

bool StreamReader::skip(size_t bytes)
{
    if (!m_stream)
        return false;

    m_stream.seekg(static_cast<std::streamoff>(bytes), std::ios::cur);
    return m_stream.good();
}

bool StreamReader::readBytes(void* dest, size_t bytes)
{
    if (!dest || !m_stream || bytes == 0)
        return false;

    m_stream.read(static_cast<char*>(dest), static_cast<std::streamsize>(bytes));
    return m_stream.good();
}

bool StreamReader::readString(std::string& out, size_t maxLength)
{
    out.clear();
    if (!m_stream)
        return false;

    char ch;
    while (out.size() < maxLength && m_stream.get(ch))
    {
        out.push_back(ch);
        if (ch == '\0') // Stop at null terminator
            break;
    }

    return m_stream.good() || m_stream.eof();
}

bool StreamReader::readLine(std::string& out, char delimiter)
{
    out.clear();
    if (!m_stream)
        return false;

    return std::getline(m_stream, out, delimiter).good();
}