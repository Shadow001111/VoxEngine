#pragma once
#include <string>
#include <unordered_map>
#include <optional>

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

	size_t registerAndGetId(const std::string& str);
	bool isRegistered(const std::string& str) const;
	std::optional<size_t> getId(const std::string& str) const;

	const std::unordered_map<std::string, size_t>& getNameToIDMap() const;

	void clear();
};