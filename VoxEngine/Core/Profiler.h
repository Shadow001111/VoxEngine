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

        double getAverageTime() const { return callCount > 0 ? totalTime / callCount : 0.0; }
        void addSample(double time);
        void reset();
    };

    static robin_hood::unordered_flat_map<const char*, ProfileData, CStrHash, CStrEqual> profileData;
    static std::chrono::steady_clock::time_point frameStartTime;
    static std::mutex profileDataMutex;

    static thread_local const char* manualProfileName;
    static thread_local ProfileCategory manualProfileCategory;
    static thread_local std::chrono::steady_clock::time_point manualProfileStartTime;

    static const char* getCategoryColor(ProfileCategory category);
    static const char* getCategoryName(ProfileCategory category);
public:
    // Note: Tracks a single timed section per thread. If called again before endProfile(), the previous data is discarded.
    static void beginProfile(const char* profileName, ProfileCategory category);
    static void endProfile();

    static const ProfileData* getProfileData(const char* name);
    static std::vector<robin_hood::pair<const char*, ProfileData>> getAllProfileData();

    static void addSample(const char* name, double duration, ProfileCategory category);

    static void resetAllProfiles();
private:
    static void printTableHeader(std::ostringstream& ss);
    static void printProfileEntry(std::ostringstream& ss, const char* name, const ProfileData& data, double frameTotalTime);
    static void printCategoryStatistics(std::ostringstream& ss, const robin_hood::unordered_flat_map<ProfileCategory, double>& categoryTotals, double frameTotalTime);
public:
    static void printProfileReport();
};

// RAII helper class for automatic profiling
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