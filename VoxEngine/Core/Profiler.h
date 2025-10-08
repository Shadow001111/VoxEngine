#pragma once
#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>
#include <limits>
#include <mutex>

enum class ProfileCategory
{
    General,
    Render,
    ChunkLoadUnload,
    ChunkBlocks,
    ChunkMesh,
    ChunkColumnData,
    TerrainGeneration,
    __COUNT__
};

// TODO: Maybe for each thread create a new instance of Profiler and then collect all of them. Idk if it is better than just mutex.
class Profiler
{
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

    static std::unordered_map<std::string, ProfileData> profileData;
    static std::chrono::high_resolution_clock::time_point frameStartTime;
    static std::mutex profileDataMutex;

    static thread_local std::string manualProfileName;
    static thread_local ProfileCategory manualProfileCategory;
    static thread_local std::chrono::high_resolution_clock::time_point manualProfileStartTime;

    static const char* getCategoryColor(ProfileCategory category);
    static const char* getCategoryName(ProfileCategory category);
public:
    // Note: Track single (smth) time per thread. If it will be called two or more times before 'endProfile', previous data will be reset.
    static void beginProfile(const char* profileName, ProfileCategory category);
    static void endProfile();

    static const ProfileData* getProfileData(const std::string& name);
    static std::vector<std::pair<std::string, ProfileData>> getAllProfileData();

    static void addSample(const std::string& name, double duration, ProfileCategory category);

    static void resetAllProfiles();
private:
    static void printTableHeader();
    static void printProfileEntry(const std::string& name, const ProfileData& data, double frameTotalTime);
    static void printCategoryStatistics(const std::unordered_map<ProfileCategory, double>& categoryTotals, double frameTotalTime);
public:
    static void printProfileReport();
};

// RAII helper class for automatic profiling
class ScopedProfiler
{
private:
    std::string name;
    ProfileCategory category;
    std::chrono::high_resolution_clock::time_point startTime;
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