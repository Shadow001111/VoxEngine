#include "StringIndexer.h"

size_t StringIndexer::getID(const char* textureName)
{
    auto it = nameToID.find(textureName);
    if (it == nameToID.end())
    {
        size_t id = nameToID.size();
        nameToID.emplace(textureName, id);
        return id;
    }
    return it->second;
}

size_t StringIndexer::getID(const std::string& textureName)
{
    auto it = nameToID.find(textureName);
    if (it == nameToID.end())
    {
        size_t id = nameToID.size();
        nameToID.emplace(textureName, id);
        return id;
    }
    return it->second;
}
