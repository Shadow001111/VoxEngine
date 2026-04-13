#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "robin_hood.h"
#include <mutex>

#include "Game/DataPackManagment/DataTypes/BlockData.h"

#include "Core/Hashes/ivec3Hasher.h"

struct StructureBlockChange
{
	BlockId block;
	uint16_t index : 12;
    uint16_t placeIfBlockIsAir : 1;
    uint16_t padding : 3;

	StructureBlockChange() = default;
	StructureBlockChange(BlockId block, uint16_t index, bool placeIfBlockIsAir)
		: block(block), index(index), placeIfBlockIsAir(placeIfBlockIsAir) {}
};

class StructureBlockManager
{
    // Map of chunk position to pending changes
    robin_hood::unordered_flat_map<glm::ivec3, std::vector<StructureBlockChange>, ivec3Hasher> pendingChanges;
    mutable std::mutex changesMutex;
public:
    std::atomic<bool> hasAnyChanges{ false };

    StructureBlockManager() = default;
    ~StructureBlockManager() = default;

    // Add a block change for a specific chunk
    void addChange(const glm::ivec3& chunkPos, BlockId block, uint16_t index, bool placeIfBlockIsAir);

    // Get and remove all pending changes for a chunk (called when chunk loads)
    std::vector<StructureBlockChange> retrieveAndClearChanges(const glm::ivec3& chunkPos);

    // Check if there are pending changes for a chunk
    bool hasPendingChanges(const glm::ivec3& chunkPos) const;

    // Clear all pending changes (for cleanup/reset)
    void clear();

    // Get total number of chunks with pending changes (for debugging)
    size_t getPendingChunkCount() const;
};
