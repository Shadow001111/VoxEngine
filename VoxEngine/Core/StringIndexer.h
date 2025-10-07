#pragma once
#include <string>
#include <unordered_map>

class StringIndexer
{
	std::unordered_map<std::string, size_t> nameToID;
public:
	StringIndexer() = default;
	~StringIndexer() = default;

	StringIndexer(const StringIndexer& other) = delete;
	StringIndexer& operator=(const StringIndexer& other) = delete;
	StringIndexer(StringIndexer&& other) = delete;
	StringIndexer& operator=(StringIndexer&& other) = delete;

	size_t getID(const char* textureName);
	size_t getID(const std::string& textureName);
};