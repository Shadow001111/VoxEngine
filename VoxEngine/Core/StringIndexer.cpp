#include "StringIndexer.h"

size_t StringIndexer::registerAndGetId(const std::string& str)
{
    auto it = nameToID.find(str);
    if (it == nameToID.end())
    {
        size_t id = nameToID.size();
        nameToID.emplace(str, id);
        return id;
    }
    return it->second;
}

bool StringIndexer::isRegistered(const std::string& str) const
{
    return nameToID.find(str) != nameToID.end();
}

std::optional<size_t> StringIndexer::getId(const std::string& str) const
{
    auto it = nameToID.find(str);
    if (it == nameToID.end())
    {
        return std::nullopt;
    }
    return it->second;
}

const std::unordered_map<std::string, size_t>& StringIndexer::getNameToIDMap() const
{
    return nameToID;
}
