#pragma once
#include "../World/ChunkPool.h"
#include "../World/ChunkLoaders/BaseChunkLoader.h"

#include <memory>
#include <mutex>
#include "robin_hood.h"

class WorldChunkManager
{
    struct BuildContainers
    {
        robin_hood::unordered_flat_set<Chunk*> blocks;
        robin_hood::unordered_flat_set<Chunk*> lights;
        robin_hood::unordered_flat_set<Chunk*> remainingLights;
        std::mutex blocksMutex;
        std::mutex lightsMutex;

        std::vector<Chunk*> lightUpdateA;
        std::vector<Chunk*> lightUpdateB;
    };

    ChunkPool chunkPool;

    BuildContainers buildContainers;

    glm::ivec3 lastChunkLoaderPos = { INT_MAX, INT_MAX, INT_MAX };
    int lastChunkLoadingDistance = -1;

    std::vector<std::unique_ptr<BaseChunkLoader>> chunkLoaders;

    mutable std::vector<Chunk*> chunksToProcess; // Used as local inside functions
public:
    WorldChunkManager() = default;
    ~WorldChunkManager();

    WorldChunkManager(const WorldChunkManager&) = delete;
    WorldChunkManager& operator=(const WorldChunkManager&) = delete;
    WorldChunkManager(WorldChunkManager&&) = delete;
    WorldChunkManager& operator=(WorldChunkManager&&) = delete;

    void init();

    void preparation(size_t chunkCount);

    void loadChunksAroundPlayer(const glm::dvec3& playerPos, int chunkLoadingDistance);

    void update();

    void sendChunkMeshesToGPU();

    Chunk* getChunkAt(const glm::ivec3& chunkPosition) const;
    bool chunkExistsAt(const glm::ivec3& chunkPosition) const;

    void startBuildingChunkBlocks();
    void startBuildingChunkLights();
    void collectChunksForLightUpdate();
    void updateChunkLights();
    void updateChunkMeshes();
    void updateChunkConnectivity();

    void rebuildAllChunkMeshes();
private:
    void loadChunk(const glm::ivec3& chunkPosition);
    void unloadChunk(const glm::ivec3& chunkPosition);

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