#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <mutex>

#include "Block.h"

#include "Core/Hashes/ivec3Hasher.h"

struct StructureBlockChange
{
	Block block;
	uint16_t index : 12;
    uint16_t placeIfBlockIsAir : 1;
    uint16_t padding : 3;

	StructureBlockChange() = default;
	StructureBlockChange(Block block, uint16_t index, bool placeIfBlockIsAir)
		: block(block), index(index), placeIfBlockIsAir(placeIfBlockIsAir) {}
};

class StructureBlockChangeManager
{
private:
    // Map of chunk position to pending changes
    std::unordered_map<glm::ivec3, std::vector<StructureBlockChange>, ivec3Hasher> pendingChanges;
    mutable std::mutex changesMutex;

public:
    StructureBlockChangeManager() = default;
    ~StructureBlockChangeManager() = default;

    // Add a block change for a specific chunk
    void addChange(const glm::ivec3& chunkPos, Block block, uint16_t index, bool placeIfBlockIsAir);

    // Get and remove all pending changes for a chunk (called when chunk loads)
    std::vector<StructureBlockChange> retrieveAndClearChanges(const glm::ivec3& chunkPos);

    // Check if there are pending changes for a chunk
    bool hasPendingChanges(const glm::ivec3& chunkPos) const;

    // Clear all pending changes (for cleanup/reset)
    void clear();

    // Get total number of chunks with pending changes (for debugging)
    size_t getPendingChunkCount() const;
};
