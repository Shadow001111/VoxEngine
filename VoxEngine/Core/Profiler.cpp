#include "Profiler.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

double Profiler::ProfileData::getAverageTime() const
{
    return callCount > 0 ? totalTime / callCount : 0.0;
}

void Profiler::ProfileData::addSample(double time)
{
    totalTime += time;
    minTime = std::min(minTime, time);
    maxTime = std::max(maxTime, time);
    callCount++;
}

void Profiler::ProfileData::reset()
{
    totalTime = 0.0;
    minTime = std::numeric_limits<double>::max();
    maxTime = 0.0;
    callCount = 0;
}

// Static member definitions
std::unordered_map<std::string, Profiler::ProfileData> Profiler::profileData;
std::chrono::high_resolution_clock::time_point Profiler::frameStartTime;
double Profiler::lastFrameTime = 0.0;

void Profiler::beginFrame()
{
    frameStartTime = std::chrono::high_resolution_clock::now();
}

void Profiler::endFrame()
{
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    lastFrameTime = std::chrono::duration<double, std::milli>(frameEndTime - frameStartTime).count();

    // Add frame time to profile data
    auto it = profileData.find("Frame Total");
    if (it != profileData.end())
    {
        it->second.addSample(lastFrameTime);
    }
    else
    {
        ProfileData data;
        data.addSample(lastFrameTime);
        profileData["Frame Total"] = data;
    }
}

void Profiler::beginProfile(const std::string& name)
{
    // This method is kept for manual profiling if needed
    // For most use cases, prefer ScopedProfiler
}

void Profiler::endProfile(const std::string& name)
{
    // This method is kept for manual profiling if needed
    // For most use cases, prefer ScopedProfiler
}

const Profiler::ProfileData* Profiler::getProfileData(const std::string& name)
{
    auto it = profileData.find(name);
    return (it != profileData.end()) ? &it->second : nullptr;
}

std::vector<std::pair<std::string, Profiler::ProfileData>> Profiler::getAllProfileData()
{
    std::vector<std::pair<std::string, ProfileData>> result;
    result.reserve(profileData.size());

    for (const auto& pair : profileData)
    {
        result.push_back(pair);
    }

    // Sort by average time (descending)
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) {
            return a.second.totalTime > b.second.totalTime;
        });

    return result;
}

void Profiler::resetAllProfiles()
{
    for (auto& pair : profileData)
    {
        pair.second.reset();
    }
}

void Profiler::printProfileReport()
{
    std::cout << "\n=== PERFORMANCE PROFILE REPORT ===\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << std::left;
    std::cout << std::setw(30) << "Function/Section"
        << std::setw(12) << "Avg (ms)"
        << std::setw(12) << "Min (ms)"
        << std::setw(12) << "Max (ms)"
        << std::setw(15) << "Total (ms)"
        << std::setw(10) << "Calls" << "\n";
    std::cout << std::string(100, '-') << "\n";

    auto sortedData = getAllProfileData();
    double totalProfiledTime = 0.0;

    for (const auto& pair : sortedData)
    {
        const std::string& name = pair.first;
        const ProfileData& data = pair.second;

        // Skip if section wasn't used at all
        if (data.callCount == 0)
        {
            continue;
        }

        // Skip frame total for percentage calculation
        if (name != "Frame Total")
        {
            totalProfiledTime += data.totalTime;
        }

        double minTime = (data.minTime == std::numeric_limits<double>::max()) ? 0.0 : data.minTime;

        std::cout << std::setw(30) << name.substr(0, 29) // Truncate long names
            << std::setw(12) << data.getAverageTime()
            << std::setw(12) << minTime
            << std::setw(12) << data.maxTime
            << std::setw(15) << data.totalTime
            << std::setw(10) << data.callCount;

        // Show percentage of total frame time if we have frame data
        const ProfileData* frameData = getProfileData("Frame Total");
        if (frameData && frameData->totalTime > 0.0 && name != "Frame Total")
        {
            double percentage = (data.totalTime / frameData->totalTime) * 100.0;
            std::cout << std::setw(8) << std::setprecision(1) << percentage << "%";
        }

        std::cout << "\n";
    }

    std::cout << std::string(100, '-') << "\n";

    // Show summary information
    const ProfileData* frameData = getProfileData("Frame Total");
    if (frameData)
    {
        std::cout << "Frame Statistics:\n";
        std::cout << "  Average FPS: " << std::setprecision(2)
            << (frameData->getAverageTime() > 0.0 ? 1000.0 / frameData->getAverageTime() : 0.0)
            << "\n";
        std::cout << "  Total frames measured: " << frameData->callCount << "\n";

        // Show worst frame performance
        if (frameData->maxTime > 0.0)
        {
            std::cout << "  Worst frame time: " << std::setprecision(4)
                << frameData->maxTime << " ms ("
                << std::setprecision(2) << (1000.0 / frameData->maxTime)
                << " FPS)" << "\n";
        }

        // Show best frame performance
        if (frameData->minTime > 0.0 && frameData->minTime != std::numeric_limits<double>::max())
        {
            std::cout << "  Best frame time: " << std::setprecision(4)
                << frameData->minTime << " ms ("
                << std::setprecision(2) << (1000.0 / frameData->minTime)
                << " FPS)" << "\n";
        }
    }

    std::cout << std::string(100, '=') << std::endl;
}

ScopedProfiler::ScopedProfiler(const std::string& profileName) : name(profileName)
{
    startTime = std::chrono::high_resolution_clock::now();
}

ScopedProfiler::~ScopedProfiler()
{
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    auto it = Profiler::profileData.find(name);
    if (it != Profiler::profileData.end())
    {
        it->second.addSample(duration);
    }
    else
    {
        Profiler::ProfileData data;
        data.addSample(duration);
        Profiler::profileData[name] = data;
    }
}
