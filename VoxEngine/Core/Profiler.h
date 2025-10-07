#pragma once
#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>
#include <limits>
#include <mutex>

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

        double getAverageTime() const;
        void addSample(double time);
        void reset();
    };
private:
    static std::unordered_map<std::string, ProfileData> profileData;
    static std::chrono::high_resolution_clock::time_point frameStartTime;
    static std::mutex profileDataMutex;
public:
    static void beginFrame();
    static void endFrame();

    static const ProfileData* getProfileData(const std::string& name);
    static std::vector<std::pair<std::string, ProfileData>> getAllProfileData();

    static void resetAllProfiles();
    static void printProfileReport();

    static void addSample(const std::string& name, double duration);
};

// RAII helper class for automatic profiling
class ScopedProfiler
{
private:
    std::string name;
    std::chrono::high_resolution_clock::time_point startTime;

public:
    ScopedProfiler(const std::string& profileName);

    ~ScopedProfiler();
};

#ifndef SCOPED_PROFILING_ENABLED
#define SCOPED_PROFILING_ENABLED 1
#endif

#if SCOPED_PROFILING_ENABLED
    #define PROFILE_SCOPE(name) ScopedProfiler _prof(name)
    #define PROFILE_FUNCTION() ScopedProfiler _prof(__FUNCTION__)
#else
    #define PROFILE_SCOPE(name) ((void)0)
    #define PROFILE_FUNCTION() ((void)0)
#endif