#include "StructureBlockChanges.h"

#include <iostream>

void StructureBlockManager::addChange(const glm::ivec3& chunkPos, BlockId block, uint16_t index, bool placeIfBlockIsAir)
{
    std::lock_guard<std::mutex> lock(changesMutex);

    // Add change to the vector for this chunk
    pendingChanges[chunkPos].emplace_back(block, index, placeIfBlockIsAir);
}

std::vector<StructureBlockChange> StructureBlockManager::retrieveAndClearChanges(const glm::ivec3& chunkPos)
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

bool StructureBlockManager::hasPendingChanges(const glm::ivec3& chunkPos) const
{
    std::lock_guard<std::mutex> lock(changesMutex);

    auto it = pendingChanges.find(chunkPos);
    return it != pendingChanges.end() && !it->second.empty();
}

void StructureBlockManager::clear()
{
    std::lock_guard<std::mutex> lock(changesMutex);
    pendingChanges.clear();
}

size_t StructureBlockManager::getPendingChunkCount() const
{
    std::lock_guard<std::mutex> lock(changesMutex);
    return pendingChanges.size();
}