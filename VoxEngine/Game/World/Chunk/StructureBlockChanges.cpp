#include "StructureBlockChanges.h"

#include <iostream>

void StructureBlockChangeManager::addChange(const glm::ivec3& chunkPos, Block block, uint16_t index, bool placeIfBlockIsAir)
{
    std::lock_guard<std::mutex> lock(changesMutex);

    // Add change to the vector for this chunk
    pendingChanges[chunkPos].emplace_back(block, index, placeIfBlockIsAir);
}

std::vector<StructureBlockChange> StructureBlockChangeManager::retrieveAndClearChanges(const glm::ivec3& chunkPos)
{
    std::lock_guard<std::mutex> lock(changesMutex);

    auto it = pendingChanges.find(chunkPos);
    if (it == pendingChanges.end())
    {
        // Return empty vector if no changes found
        return {};
    }

    // Move the vector out and erase the entry
    std::vector<StructureBlockChange> result = std::move(it->second);
    pendingChanges.erase(it);
    return result;
}

bool StructureBlockChangeManager::hasPendingChanges(const glm::ivec3& chunkPos) const
{
    std::lock_guard<std::mutex> lock(changesMutex);

    auto it = pendingChanges.find(chunkPos);
    return it != pendingChanges.end() && !it->second.empty();
}

void StructureBlockChangeManager::clear()
{
    std::lock_guard<std::mutex> lock(changesMutex);
    pendingChanges.clear();
}

size_t StructureBlockChangeManager::getPendingChunkCount() const
{
    std::lock_guard<std::mutex> lock(changesMutex);
    return pendingChanges.size();
}