#pragma once

#include <bitset>
#include <vector>

#include <glm/glm.hpp>
#include "robin_hood.h"
#include "Core/Hashes/ivec3Hasher.h"

class BaseChunkLoader
{
protected:
    class Region
    {
    public:
        static constexpr int REGION_SIZE_LOG2 = 4;
        static constexpr int REGION_SIZE = 1 << REGION_SIZE_LOG2;
        static constexpr int REGION_MASK = REGION_SIZE - 1;
        static constexpr int REGION_VOLUME = REGION_SIZE * REGION_SIZE * REGION_SIZE;
        using Bitset = std::bitset<REGION_VOLUME>;
    private:
        Bitset bits_{};
    public:
        static glm::ivec3 transformPositionToRegion(const glm::ivec3& pos) noexcept
        {
            return pos >> REGION_SIZE_LOG2;
        }

        static glm::ivec3 transformRegionToOrigin(const glm::ivec3& region) noexcept
        {
            return region << REGION_SIZE_LOG2;
        }

        static int getIndexFromPosition(const glm::ivec3& pos) noexcept
        {
            const glm::ivec3 local = pos & glm::ivec3(REGION_MASK);

            return (local.x << (REGION_SIZE_LOG2 * 2)) | (local.y << REGION_SIZE_LOG2) | local.z;
        }

        static glm::ivec3 getPositionFromIndex(const glm::ivec3& region, int index) noexcept
        {
            const glm::ivec3 origin = region << REGION_SIZE_LOG2;

            const int x = index >> (REGION_SIZE_LOG2 * 2);
            const int y = (index >> REGION_SIZE_LOG2) & REGION_MASK;
            const int z = index & REGION_MASK;

            return
            {
                origin.x + x,
                origin.y + y,
                origin.z + z
            };
        }

        void setIndex(int index) noexcept
        {
            bits_.set(index);
        }

        void appendAllPositions(const glm::ivec3& region, std::vector<glm::ivec3>& out) const
        {
            appendPositionsFromBits(region, bits_, out);
        }

        static void appendPositionsFromBits(const glm::ivec3& region,
            const Bitset& bits,
            std::vector<glm::ivec3>& out)
        {
            for (int i = 0; i < REGION_VOLUME; ++i)
            {
                if (bits.test(i))
                {
                    out.push_back(getPositionFromIndex(region, i));
                }
            }
        }

        const Bitset& bits() const noexcept
        {
            return bits_;
        }

        Bitset& bits() noexcept
        {
            return bits_;
        }
    };

    using RegionMap = robin_hood::unordered_flat_map<glm::ivec3, Region, ivec3Hasher>;

private:
    std::vector<glm::ivec3> chunkLoaderPositions;
    RegionMap loaded;
    RegionMap prevLoaded;
    std::vector<glm::ivec3> toLoad;
    std::vector<glm::ivec3> toUnload;

public:
    virtual ~BaseChunkLoader() = default;

    void update(const glm::ivec3& playerChunkPosition, int loadRadius);

    const std::vector<glm::ivec3>& getChunksToLoad() const noexcept
    {
        return toLoad;
    }

    const std::vector<glm::ivec3>& getChunksToUnload() const noexcept
    {
        return toUnload;
    }

protected:
    virtual void getPositionsToLoad(const glm::ivec3& playerChunkPosition,
        int loadRadius,
        std::vector<glm::ivec3>& positions) = 0;

private:
    void computeDiffs();
};