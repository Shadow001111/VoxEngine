#pragma once

// TODO: Maybe add 'emplace' method
template<typename T>
class BufferStreamWriter
{
	T* destination = nullptr;
public:
	BufferStreamWriter(T* destination) : destination(destination)
	{
	}

	void writeSingle(const T* source)
	{
		std::memcpy(destination, source, sizeof(T));
		destination++;
	}

	void writeSingle(const T& source)
	{
		std::memcpy(destination, &source, sizeof(T));
		destination++;
	}

	void writeMultiple(const T* source, size_t count)
	{
		std::memcpy(destination, source, count * sizeof(T));
		destination += count;
	}

	void writeMultiple(const T& source, size_t count)
	{
		std::memcpy(destination, &source, count * sizeof(T));
		destination += count;
	}

	const T* getDestination() const { return destination; }
};