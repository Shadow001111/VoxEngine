#pragma once
#include "Game/World/ChunkPool.h"
#include "Game/World/ChunkLoaders/BaseChunkLoader.h"

#include "Core/Multithreading/ThreadPool.h"

#include <memory>
#include <mutex>
#include "robin_hood.h"

class WorldChunkManager
{
    struct BuildContainers
    {
        std::vector<Chunk*> blocks;
        std::vector<Chunk*> lightsIncoming; // Producers (Workers) push here
        std::vector<Chunk*> lightsProcessing; // Consumer (Main Thread) processes here
        TracyLockable(std::mutex, lightsMutex);

        std::vector<Chunk*> lightUpdateA;
        std::vector<Chunk*> lightUpdateB;
    };

    struct ChunkCompressionTestResult
    {
        std::string name;
        size_t chunkCount = 0;
        size_t originalSize = 0;
        size_t compressedSize = 0;
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
    void updateChunksLight();
    void updateChunkMeshes();
    void updateChunkConnectivity();

    // Debug

    void rebuildAllChunkMeshes();
    void chunkCompressionAlgorithmsTest();

    template<class CompressFn>
    ChunkCompressionTestResult runChunkCompressionTest(std::string_view testName, CompressFn&& compressFn)
    {
        std::vector<Chunk*> chunksToProcess;
        chunksToProcess.reserve(1024);

        for (const auto& [_, chunkRegion] : Chunk::managerInstances->chunkRegion.getRegionMap())
        {
            for (Chunk* chunk : chunkRegion->chunks)
            {
                if (chunk)
                    chunksToProcess.push_back(chunk);
            }
        }

        std::atomic<size_t> totalCompressedSizeAtomic{ 0 };

        ParallelUtils::parallelForEach(chunksToProcess, 1,
            [&totalCompressedSizeAtomic, &compressFn](Chunk* chunk)
            {
                const size_t compressedSize = std::invoke(compressFn, *chunk);
                totalCompressedSizeAtomic.fetch_add(compressedSize, std::memory_order_relaxed);
            });

        const size_t totalOriginalSize = chunksToProcess.size() * CHUNK_VOLUME * sizeof(BlockId);
        const size_t totalCompressedSize = totalCompressedSizeAtomic.load(std::memory_order_relaxed);

        ChunkCompressionTestResult result;
        result.name = std::string(testName);
        result.chunkCount = chunksToProcess.size();
        result.originalSize = totalOriginalSize;
        result.compressedSize = totalCompressedSize;

        const float compressedPercentage =
            totalOriginalSize ? static_cast<float>(totalCompressedSize) / totalOriginalSize * 100.0f : 0.0f;

        const size_t compressionSavingsMB =
            (totalOriginalSize > totalCompressedSize)
            ? (totalOriginalSize - totalCompressedSize) / (1024 * 1024)
            : 0;

		std::cout << "----------------------------------------\n";
        std::cout << "'" << testName << "' test results:\n";
        std::cout << "Total original size: " << totalOriginalSize / (1024 * 1024) << " MB\n";
        std::cout << "Total compressed size: " << totalCompressedSize / (1024 * 1024) << " MB\n";
        std::cout << "Compression percentage: " << compressedPercentage << "%\n";
        std::cout << "Compression savings: " << compressionSavingsMB << " MB\n";
		std::cout << "----------------------------------------\n";

        return result;
    }
private:
    void loadChunk(const glm::ivec3& chunkPosition);
    void unloadChunk(const glm::ivec3& chunkPosition);
    void collectChunkNeighbors(const glm::ivec3& chunkPosition, std::array<Chunk*, 27>& neighbors) const;

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