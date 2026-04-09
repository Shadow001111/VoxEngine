#pragma once
#include <chrono>
#include "robin_hood.h"
#include <vector>
#include <limits>
#include <mutex>
#include <sstream>
#include <cstring>
#include "Debug.h"

enum class ProfileCategory
{
    General,
    Render,
    ChunkLoadUnload,
    ChunkBlocks,
    ChunkLight,
    ChunkMesh,
    ChunkColumnData,
    TerrainGeneration,
    __COUNT__
};

struct CStrHash
{
    size_t operator()(const char* s) const noexcept
    {
        // FNV-1a 64-bit
        size_t hash = 14695981039346656037ULL;
        while (*s)
        {
            hash ^= static_cast<unsigned char>(*s++);
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

struct CStrEqual
{
    bool operator()(const char* a, const char* b) const noexcept
    {
        return std::strcmp(a, b) == 0;
    }
};

class Profiler
{
    struct ProfileData
    {
        double totalTime = 0.0;
        double minTime = std::numeric_limits<double>::max();
        double maxTime = 0.0;
        uint64_t callCount = 0;
        ProfileCategory category = ProfileCategory::General;

        double getAverageTime() const noexcept { return callCount > 0 ? totalTime / callCount : 0.0; }
        void addSample(double time) noexcept;
        void reset() noexcept;
    };

    using ProfileDataMap = robin_hood::unordered_flat_map<const char*, ProfileData, CStrHash, CStrEqual>;
    using NameData = std::pair<const char*, ProfileData>;

    struct ThreadLocalData
    {
        ProfileDataMap profileData;
        std::mutex mtx;

        ThreadLocalData();
    };

    static std::vector<ThreadLocalData*> threadRegistry;
    static std::mutex threadRegistryMtx;

    static const char* getCategoryColor(ProfileCategory category);
    static const char* getCategoryName(ProfileCategory category);
    static std::vector<NameData> getMergeClearProfileData();
public:
    static void registerThread(ThreadLocalData* data);

    static void addSample(const char* name, double duration, ProfileCategory category);

    static void printProfileReport();
private:
    static void printTableHeader(std::ostringstream& ss);
    static void printProfileEntry(std::ostringstream& ss, const char* name, const ProfileData& data, double frameTotalTime);
    static void printCategoryStatistics(std::ostringstream& ss, const robin_hood::unordered_flat_map<ProfileCategory, double>& categoryTotals, double frameTotalTime);
};

class ScopedProfiler
{
private:
    const char* name;
    ProfileCategory category;
    std::chrono::steady_clock::time_point startTime;
public:
    ScopedProfiler(const char* profileName, ProfileCategory category);
    ~ScopedProfiler();
};

#if PROFILING_ENABLED
#define PROFILE_SCOPE(name, category) ScopedProfiler _prof(name, category)
#else
#define PROFILE_SCOPE(name, category)
#endif