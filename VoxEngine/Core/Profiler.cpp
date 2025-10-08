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
std::mutex Profiler::profileDataMutex;

thread_local std::string Profiler::manualProfileName;
thread_local ProfileCategory Profiler::manualProfileCategory;
thread_local std::chrono::high_resolution_clock::time_point Profiler::manualProfileStartTime;

namespace ProfilerColors
{
    constexpr const char* RESET = "\033[0m";

    constexpr const char* GENERAL = "\033[90m";   // Gray
    constexpr const char* RENDER = "\033[31m";   // Red
    constexpr const char* CHUNK_LOAD_UNLOAD = "\033[33m";   // Yellow
    constexpr const char* CHUNK_BLOCKS = "\033[32m";   // Green
    constexpr const char* CHUNK_MESH = "\033[36m";   // Cyan
    constexpr const char* TERRAIN_GENERATION = "\033[35m";   // Magenta
}

const char* Profiler::getCategoryColor(ProfileCategory category)
{
    switch (category)
    {
    case ProfileCategory::Render: return ProfilerColors::RENDER;
    case ProfileCategory::ChunkLoadUnload: return ProfilerColors::CHUNK_LOAD_UNLOAD;
    case ProfileCategory::ChunkBlocks: return ProfilerColors::CHUNK_BLOCKS;
    case ProfileCategory::ChunkMesh: return ProfilerColors::CHUNK_MESH;
    case ProfileCategory::TerrainGeneration: return ProfilerColors::TERRAIN_GENERATION;
    case ProfileCategory::General:
    default:                           return ProfilerColors::GENERAL;
    }
}

const char* Profiler::getCategoryName(ProfileCategory category)
{
    switch (category)
    {
    case ProfileCategory::Render: return "Render";
    case ProfileCategory::ChunkLoadUnload: return "Chunk Load/Unload";
    case ProfileCategory::ChunkBlocks: return "Chunk Blocks";
    case ProfileCategory::ChunkMesh: return "Chunk Mesh";
    case ProfileCategory::TerrainGeneration: return "Terrain Generation";
    case ProfileCategory::General: return "General";
    default: return "Unknown";
    }
}

void Profiler::beginProfile(const char* profileName, ProfileCategory category)
{
    manualProfileName = std::string(profileName);
    manualProfileCategory = category;
    manualProfileStartTime = std::chrono::high_resolution_clock::now();
}

void Profiler::endProfile()
{
    auto endTime = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(endTime - manualProfileStartTime).count();
    Profiler::addSample(manualProfileName, duration, manualProfileCategory);
}

const Profiler::ProfileData* Profiler::getProfileData(const std::string& name)
{
    std::lock_guard<std::mutex> lock(profileDataMutex);
    auto it = profileData.find(name);
    return (it != profileData.end()) ? &it->second : nullptr;
}

std::vector<std::pair<std::string, Profiler::ProfileData>> Profiler::getAllProfileData()
{
    std::vector<std::pair<std::string, ProfileData>> result;

    {
        std::lock_guard<std::mutex> lock(profileDataMutex);
        result.reserve(profileData.size());
        for (const auto& pair : profileData)
        {
            result.push_back(pair);
        }
    }

    // Sort by average time (descending)
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) {
            return a.second.totalTime > b.second.totalTime;
        });

    return result;
}

void Profiler::addSample(const std::string& name, double duration, ProfileCategory category)
{
    std::lock_guard<std::mutex> lock(profileDataMutex);
    auto it = profileData.find(name);
    if (it != profileData.end())
    {
        it->second.addSample(duration);
    }
    else
    {
        ProfileData data;
        data.category = category;
        data.addSample(duration);
        profileData.emplace(name, data);
    }
}

void Profiler::resetAllProfiles()
{
    std::lock_guard<std::mutex> lock(profileDataMutex);
    for (auto& pair : profileData)
    {
        pair.second.reset();
    }
}

// ANSI Color codes for console output
namespace ProfilerReport
{
    constexpr int COL_NAME = 30;
    constexpr int COL_AVG = 12;
    constexpr int COL_MIN = 12;
    constexpr int COL_MAX = 12;
    constexpr int COL_TOTAL = 15;
    constexpr int COL_CALLS = 10;
    constexpr int COL_PERCENT = 8;

    constexpr int TOTAL_WIDTH = COL_NAME + COL_AVG + COL_MIN + COL_MAX + COL_TOTAL + COL_CALLS + COL_PERCENT;
}

void Profiler::printTableHeader()
{
    std::cout << std::left
              << std::setw(ProfilerReport::COL_NAME) << "Function/Section"
              << std::setw(ProfilerReport::COL_AVG) << "Avg (ms)"
              << std::setw(ProfilerReport::COL_MIN) << "Min (ms)"
              << std::setw(ProfilerReport::COL_MAX) << "Max (ms)"
              << std::setw(ProfilerReport::COL_TOTAL) << "Total (ms)"
              << std::setw(ProfilerReport::COL_CALLS) << "Calls" << "\n"
              << std::string(ProfilerReport::TOTAL_WIDTH, '-') << "\n";
}

void Profiler::printProfileEntry(const std::string& name, const ProfileData& data, double frameTotalTime)
{
    if (data.callCount == 0) return;

    double minTime = (data.minTime == std::numeric_limits<double>::max()) ? 0.0 : data.minTime;
    const char* color = getCategoryColor(data.category);

    std::cout << color
              << std::setw(ProfilerReport::COL_NAME)  << name.substr(0, ProfilerReport::COL_NAME - 1)
              << std::setw(ProfilerReport::COL_AVG)   << data.getAverageTime()
              << std::setw(ProfilerReport::COL_MIN)   << minTime
              << std::setw(ProfilerReport::COL_MAX)   << data.maxTime
              << std::setw(ProfilerReport::COL_TOTAL) << data.totalTime
              << std::setw(ProfilerReport::COL_CALLS) << data.callCount;

    // Add percentage if not the frame total itself
    double percentage = (data.totalTime / frameTotalTime) * 100.0;
    std::cout << std::setw(ProfilerReport::COL_PERCENT) << std::setprecision(1) << percentage << "%";

    std::cout << ProfilerColors::RESET << "\n";
}

void Profiler::printCategoryStatistics(const std::unordered_map<ProfileCategory, double>& categoryTotals, double frameTotalTime)
{
    if (categoryTotals.empty()) return;

    std::cout << "Category Statistics:\n";
    std::cout << std::left;
    std::cout << std::setw(ProfilerReport::COL_NAME) << "Category"
        << std::setw(ProfilerReport::COL_TOTAL) << "Total (ms)"
        << "% of Frame" << "\n";
    std::cout << std::string(ProfilerReport::TOTAL_WIDTH, '-') << "\n";

    // Sort categories by total time (descending)
    std::vector<std::pair<ProfileCategory, double>> sortedCategories(categoryTotals.begin(), categoryTotals.end());
    std::sort(sortedCategories.begin(), sortedCategories.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& pair : sortedCategories)
    {
        ProfileCategory category = pair.first;
        double totalTime = pair.second;

        const char* color = getCategoryColor(category);
        const char* name = getCategoryName(category);

        std::cout << color << std::left;
        std::cout << std::setw(ProfilerReport::COL_NAME) << name
            << std::setw(ProfilerReport::COL_TOTAL) << std::setprecision(4) << totalTime;

        // Show percentage of frame time
        double percentage = (totalTime / frameTotalTime) * 100.0;
        std::cout << std::setprecision(1) << percentage << "%";

        std::cout << ProfilerColors::RESET << "\n";
    }
}

void Profiler::printProfileReport()
{
    //Save current iostream state
    std::ios_base::fmtflags originalFlags = std::cout.flags();
    std::streamsize originalPrecision = std::cout.precision();

    //
    std::cout << "\n===[PERFORMANCE PROFILE REPORT]" << std::string(ProfilerReport::TOTAL_WIDTH - 32, '=') << "\n";
    std::cout << std::fixed << std::setprecision(4);

    printTableHeader();

    auto sortedData = getAllProfileData();
    const ProfileData* frameData = getProfileData("Frame Total");

    // Track time per category and total time
    std::unordered_map<ProfileCategory, double> categoryTotals;
    double totalTime = 0.0f;

    for (const auto& pair : sortedData)
    {
        const auto& name = pair.first;
        const auto& data = pair.second;

        categoryTotals[data.category] += data.totalTime;
        totalTime += data.totalTime;
    }

    if (totalTime > 0.0f)
    {
        // Print all profile entries
        std::cout << "Entries Statistics:\n";
        for (const auto& pair : sortedData)
        {
            const auto& name = pair.first;
            const auto& data = pair.second;

            printProfileEntry(name, data, totalTime);
        }

        std::cout << std::string(ProfilerReport::TOTAL_WIDTH, '=') << "\n";

        printCategoryStatistics(categoryTotals, totalTime);
    }

    std::cout << std::string(ProfilerReport::TOTAL_WIDTH, '=') << std::endl;

    // Restore original iostream state
    std::cout.flags(originalFlags);
    std::cout.precision(originalPrecision);

    //
    Profiler::resetAllProfiles();
}


ScopedProfiler::ScopedProfiler(const char* profileName, ProfileCategory category) :
    name(profileName), category(category), startTime(std::chrono::high_resolution_clock::now())
{
}

ScopedProfiler::~ScopedProfiler()
{
    auto endTime = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    Profiler::addSample(name, duration, category);
}
