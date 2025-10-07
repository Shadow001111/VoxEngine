#pragma once
#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>
#include <limits>
#include <mutex>

// ANSI Color codes for console output
namespace ProfilerColors
{
    constexpr const char* RESET = "\033[0m";

    constexpr const char* FRAME_TOTAL = "\033[37m";  // White
    constexpr const char* GENERAL = "\033[90m";   // Gray

    constexpr const char* RENDER = "\033[31m";   // Red
    constexpr const char* CHUNK_LOAD_UNLOAD = "\033[33m";   // Yellow
    constexpr const char* CHUNK_BLOCKS = "\033[32m";   // Green
    constexpr const char* CHUNK_MESH = "\033[36m";   // Cyan
    constexpr const char* TERRAIN_GENERATION = "\033[35m";   // Magenta
}

enum class ProfileCategory
{
    FrameTotal,
    General,
    Render,
    ChunkLoadUnload,
    ChunkBlocks,
    ChunkMesh,
    TerrainGeneration
};

// TODO: Maybe for each thread create a new instance of Profiler and then collect all of them. Idk if it is better than just mutex.
class Profiler
{
public:
    struct ProfileData
    {
        double totalTime = 0.0;
        double minTime = std::numeric_limits<double>::max();
        double maxTime = 0.0;
        uint64_t callCount = 0;
        ProfileCategory category = ProfileCategory::General;

        double getAverageTime() const;
        void addSample(double time);
        void reset();
    };
private:
    static std::unordered_map<std::string, ProfileData> profileData;
    static std::chrono::high_resolution_clock::time_point frameStartTime;
    static std::mutex profileDataMutex;

    static const char* getCategoryColor(ProfileCategory category);
public:
    static void beginFrame();
    static void endFrame();

    static const ProfileData* getProfileData(const std::string& name);
    static std::vector<std::pair<std::string, ProfileData>> getAllProfileData();

    static void resetAllProfiles();
    static void printProfileReport();

    static void addSample(const std::string& name, double duration, ProfileCategory category);
};

// RAII helper class for automatic profiling
class ScopedProfiler
{
private:
    std::string name;
    std::chrono::high_resolution_clock::time_point startTime;
    ProfileCategory category;
public:
    ScopedProfiler(const char* profileName, ProfileCategory category);

    ~ScopedProfiler();
};

#ifndef SCOPED_PROFILING_ENABLED
#define SCOPED_PROFILING_ENABLED 1
#endif

#if SCOPED_PROFILING_ENABLED
    #define PROFILE_SCOPE(name, category) ScopedProfiler _prof(name, category)
#else
    #define PROFILE_SCOPE(name) ((void)0)
#endif