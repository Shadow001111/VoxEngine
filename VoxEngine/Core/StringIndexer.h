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

	// Returns size_t max value if ID is invalid
	size_t getID(const char* textureName);
	// Returns size_t max value if ID is invalid
	size_t getID(const std::string& textureName);

	const std::unordered_map<std::string, size_t>& getNameToIDMap() const;
};