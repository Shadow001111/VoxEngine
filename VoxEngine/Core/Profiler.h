#pragma once
#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>
#include <limits>

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
    static double lastFrameTime;

public:
    static void beginFrame();
    static void endFrame();
    static double getLastFrameTime() { return lastFrameTime; }

    static void beginProfile(const std::string& name);
    static void endProfile(const std::string& name);

    static const ProfileData* getProfileData(const std::string& name);
    static std::vector<std::pair<std::string, ProfileData>> getAllProfileData();

    static void resetAllProfiles();
    static void printProfileReport();

    // Allow ScopedProfiler access to private members
    friend class ScopedProfiler;
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

// Convenience macros for easy profiling
#define PROFILE_SCOPE(name) ScopedProfiler _prof(name)
#define PROFILE_FUNCTION() ScopedProfiler _prof(__FUNCTION__)