#pragma once
#include "../World/ChunkPool.h"
#include "../World/ChunkRegion.h"
#include "../World/ChunkLoaders/BaseChunkLoader.h"

#include "Core/Hashes/ivec3Hasher.h"

#include <memory>
#include <mutex>
#include "robin_hood.h"

class WorldChunkManager
{
    struct BuildContainers
    {
        robin_hood::unordered_flat_set<Chunk*> blocks;
        robin_hood::unordered_flat_set<Chunk*> lights;
        std::mutex blocksMutex;
        std::mutex lightsMutex;

        std::vector<Chunk*> lightUpdateA;
        std::vector<Chunk*> lightUpdateB;
    };

    ChunkPool chunkPool;
    robin_hood::unordered_flat_map<glm::ivec3, Chunk*, ivec3Hasher> chunks;
    
    FixedArenaObjectPool<ChunkRegion, 4> chunkRegionPool;
    robin_hood::unordered_flat_map<glm::ivec3, ChunkRegion*, ivec3Hasher> chunkRegions;

    BuildContainers buildContainers;

    glm::ivec3 lastChunkLoaderPos = { INT_MAX, INT_MAX, INT_MAX };
    int lastChunkLoadingDistance = -1;

    std::vector<std::unique_ptr<BaseChunkLoader>> chunkLoaders;
public:
    WorldChunkManager();
    ~WorldChunkManager() = default;

    WorldChunkManager(const WorldChunkManager&) = delete;
    WorldChunkManager& operator=(const WorldChunkManager&) = delete;
    WorldChunkManager(WorldChunkManager&&) = delete;
    WorldChunkManager& operator=(WorldChunkManager&&) = delete;

    void preparation(size_t chunkCount);

    void loadChunks(const glm::dvec3& playerPos, int chunkLoadingDistance);

    void update();

    void sendChunkMeshesToGPU();

    Chunk* getChunkAt(const glm::ivec3& position) const;
    bool chunkExistsAt(const glm::ivec3& position) const;
    size_t getLoadedChunksCount() const { return chunks.size(); };

    void startBuildingChunkBlocks();
    void startBuildingChunkLights();
    void collectChunksNeedingLightUpdate();
    void updateChunkLights();
    void updateChunkMeshes();

    void rebuildAllChunkMeshes();

    const auto& getAllChunks() const { return chunks; }
    const auto& getAllChunkRegions() const { return chunkRegions; }
    size_t getRegionCount() const { return chunkRegions.size(); }
private:
    void loadChunk(const glm::ivec3& position);
    void unloadChunk(const glm::ivec3& position);

    template<typename T, typename... Args>
    T* createChunkLoader(Args&&... args)
    {
        static_assert(std::is_base_of<BaseChunkLoader, T>::value, "T must derive from BaseChunkLoader");
        static_assert(!std::is_same_v<BaseChunkLoader, T>, "T mustn't be BaseChunkLoader");

        std::unique_ptr<T> chl = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = chl.get();
        chunkLoaders.push_back(std::move(chl));
        return raw;
    }
};