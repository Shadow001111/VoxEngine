#include "Profiler.h"

#include <iostream>
#include <iomanip>
#include <algorithm>

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
robin_hood::unordered_flat_map<const char*, Profiler::ProfileData, CStrHash, CStrEqual> Profiler::profileData;
std::chrono::steady_clock::time_point Profiler::frameStartTime;
std::mutex Profiler::profileDataMutex;

thread_local const char* Profiler::manualProfileName = nullptr;
thread_local ProfileCategory Profiler::manualProfileCategory;
thread_local std::chrono::steady_clock::time_point Profiler::manualProfileStartTime;

namespace
{
    std::string formatTimeCell(double milliseconds)
    {
        double value = milliseconds;
        const char* unit = " ms";

        if (value < 1.0)
        {
            value *= 1000.0;
            unit = " us";

            if (value < 1.0)
            {
                value *= 1000.0;
                unit = " ns";
            }
        }

        int precision = 3;
        if (value >= 1000.0)      precision = 0;
        else if (value >= 100.0)  precision = 1;
        else if (value >= 10.0)   precision = 2;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        if (precision == 0 && value < 10000.0)
        {
            oss << " ";
        }
        oss << unit;
        return oss.str();
    }
}

namespace ProfilerColors
{
    enum class Color
    {
        Default = 39,
        Black = 30,
        Red = 31,
        Green = 32,
        Yellow = 33,
        Blue = 34,
        Magenta = 35,
        Cyan = 36,
        White = 37,

        BrightBlack = 90,
        BrightRed = 91,
        BrightGreen = 92,
        BrightYellow = 93,
        BrightBlue = 94,
        BrightMagenta = 95,
        BrightCyan = 96,
        BrightWhite = 97
    };

    enum class Style
    {
        None = 0,
        Bold = 1,
        Dim = 2,
        Italic = 3,
        Underline = 4,
        Blink = 5,
        Invert = 7,
        Hidden = 8,
        Strike = 9
    };

    std::string make_ansi_prefix(Color color = Color::Default, Color bg = Color::Default, std::initializer_list<Style> styles = {})
    {
        std::string result = "\033[";
        bool first = true;

        auto append_code = [&](int code)
            {
                if (!first)
                {
                    result += ';';
                }
                result += std::to_string(code);
                first = false;
            };

        append_code(static_cast<int>(color));
        append_code(static_cast<int>(bg) + 10);
        for (auto style : styles)
        {
            if (style != Style::None)
            {
                append_code(static_cast<int>(style));
            }
        }

        result += 'm';
        return result;
    }

    const char* RESET = "\033[0m";

    const std::string DEFAULT = make_ansi_prefix(Color::Default);

    const std::string RED = make_ansi_prefix(Color::Red);
    const std::string GREEN = make_ansi_prefix(Color::Green);
    const std::string YELLOW = make_ansi_prefix(Color::Yellow);
    const std::string BLUE = make_ansi_prefix(Color::Blue);
    const std::string MAGENTA = make_ansi_prefix(Color::Magenta);
    const std::string CYAN = make_ansi_prefix(Color::Cyan);

    const std::string BRIGHT_RED = make_ansi_prefix(Color::BrightRed);
    const std::string BRIGHT_GREEN = make_ansi_prefix(Color::BrightGreen);
    const std::string BRIGHT_YELLOW = make_ansi_prefix(Color::BrightYellow);
    const std::string BRIGHT_BLUE = make_ansi_prefix(Color::BrightBlue);
    const std::string BRIGHT_MAGENTA = make_ansi_prefix(Color::BrightMagenta);
    const std::string BRIGHT_CYAN = make_ansi_prefix(Color::BrightCyan);

    const std::string BRIGHT_WHITE = make_ansi_prefix(Color::BrightWhite);
    const std::string GRAY = make_ansi_prefix(Color::BrightBlack);
}

const char* Profiler::getCategoryColor(ProfileCategory category)
{
    switch (category)
    {
    case ProfileCategory::General:              return ProfilerColors::GRAY.c_str();
    case ProfileCategory::Render:               return ProfilerColors::RED.c_str();
    case ProfileCategory::ChunkLoadUnload:      return ProfilerColors::YELLOW.c_str();
    case ProfileCategory::ChunkBlocks:          return ProfilerColors::GREEN.c_str();
    case ProfileCategory::ChunkLight:           return ProfilerColors::BRIGHT_RED.c_str();
    case ProfileCategory::ChunkMesh:            return ProfilerColors::CYAN.c_str();
    case ProfileCategory::TerrainGeneration:    return ProfilerColors::MAGENTA.c_str();
    case ProfileCategory::ChunkColumnData:      return ProfilerColors::BLUE.c_str();
    default:                                    return ProfilerColors::GRAY.c_str();
    }
}

const char* Profiler::getCategoryName(ProfileCategory category)
{
    switch (category)
    {
    case ProfileCategory::General:          return "General";
    case ProfileCategory::Render:           return "Render";
    case ProfileCategory::ChunkLoadUnload:  return "Chunk Load/Unload";
    case ProfileCategory::ChunkBlocks:      return "Chunk Blocks";
    case ProfileCategory::ChunkLight:       return "Chunk Light";
    case ProfileCategory::ChunkMesh:        return "Chunk Mesh";
    case ProfileCategory::TerrainGeneration:return "Terrain Generation";
    case ProfileCategory::ChunkColumnData:  return "ChunkColumnData";
    default:                                return "Unknown";
    }
}

void Profiler::beginProfile(const char* profileName, ProfileCategory category)
{
#if PROFILING_ENABLED
    manualProfileName = profileName;
    manualProfileCategory = category;
    manualProfileStartTime = std::chrono::high_resolution_clock::now();
#endif
}

void Profiler::endProfile()
{
#if PROFILING_ENABLED
    auto endTime = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(endTime - manualProfileStartTime).count();
    Profiler::addSample(manualProfileName, duration, manualProfileCategory);
#endif
}

const Profiler::ProfileData* Profiler::getProfileData(const char* name)
{
    std::lock_guard<std::mutex> lock(profileDataMutex);
    auto it = profileData.find(name);
    return (it != profileData.end()) ? &it->second : nullptr;
}

std::vector<robin_hood::pair<const char*, Profiler::ProfileData>> Profiler::getAllProfileData()
{
    std::vector<robin_hood::pair<const char*, ProfileData>> result;

    {
        std::lock_guard<std::mutex> lock(profileDataMutex);
        result.reserve(profileData.size());
        for (const auto& pair : profileData)
        {
            result.emplace_back(pair.first, pair.second);
        }
    }

    // Sort by total time (descending)
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) {
            return a.second.totalTime > b.second.totalTime;
        });

    return result;
}

void Profiler::addSample(const char* name, double duration, ProfileCategory category)
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

namespace ProfilerReport
{
    constexpr int COL_NAME = 35;
    constexpr int COL_AVG = 11;
    constexpr int COL_MIN = 11;
    constexpr int COL_MAX = 11;
    constexpr int COL_TOTAL = 11;
    constexpr int COL_CALLS = 8;
    constexpr int COL_PERCENT = 7;

    constexpr int TOTAL_WIDTH = COL_NAME + COL_AVG + COL_MIN + COL_MAX + COL_TOTAL + COL_CALLS + COL_PERCENT;
}

void Profiler::printTableHeader(std::ostringstream& ss)
{
    ss << std::left
        << std::setw(ProfilerReport::COL_NAME) << "Function/Section"
        << std::right
        << std::setw(ProfilerReport::COL_AVG) << "Avg"
        << std::setw(ProfilerReport::COL_MIN) << "Min"
        << std::setw(ProfilerReport::COL_MAX) << "Max"
        << std::setw(ProfilerReport::COL_TOTAL) << "Total"
        << std::setw(ProfilerReport::COL_CALLS) << "Calls"
        << std::setw(ProfilerReport::COL_PERCENT) << "Percent"
        << "\n" << std::string(ProfilerReport::TOTAL_WIDTH, '-') << "\n";
}

void Profiler::printProfileEntry(std::ostringstream& ss, const char* name, const ProfileData& data, double frameTotalTime)
{
    if (data.callCount == 0) return;

    double minTime = (data.minTime == std::numeric_limits<double>::max()) ? 0.0 : data.minTime;
    const char* color = getCategoryColor(data.category);

    // Truncate name to fit column without allocating a std::string
    char truncated[ProfilerReport::COL_NAME];
    std::strncpy(truncated, name, ProfilerReport::COL_NAME - 1);
    truncated[ProfilerReport::COL_NAME - 1] = '\0';

    ss << color
        << std::setw(ProfilerReport::COL_NAME) << std::left << truncated
        << std::setw(ProfilerReport::COL_AVG) << std::right << formatTimeCell(data.getAverageTime())
        << std::setw(ProfilerReport::COL_MIN) << formatTimeCell(minTime)
        << std::setw(ProfilerReport::COL_MAX) << formatTimeCell(data.maxTime)
        << std::setw(ProfilerReport::COL_TOTAL) << formatTimeCell(data.totalTime)
        << std::setw(ProfilerReport::COL_CALLS) << data.callCount;

    double percentage = (data.totalTime / frameTotalTime) * 100.0;
    ss << std::setw(ProfilerReport::COL_PERCENT) << percentage << "%";

    ss << ProfilerColors::RESET << "\n";
}

void Profiler::printCategoryStatistics(std::ostringstream& ss, const robin_hood::unordered_flat_map<ProfileCategory, double>& categoryTotals, double frameTotalTime)
{
    if (categoryTotals.empty()) return;

    ss << "Category Statistics:\n";
    ss << std::left;
    ss << std::setw(ProfilerReport::COL_NAME) << "Category"
        << std::setw(ProfilerReport::COL_TOTAL) << "Total"
        << "% of Frame\n";
    ss << std::string(ProfilerReport::TOTAL_WIDTH, '-') << "\n";

    // Sort categories by total time (descending)
    std::vector<robin_hood::pair<ProfileCategory, double>> sortedCategories(categoryTotals.begin(), categoryTotals.end());
    std::sort(sortedCategories.begin(), sortedCategories.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& pair : sortedCategories)
    {
        ProfileCategory category = pair.first;
        double totalTime = pair.second;

        const char* color = getCategoryColor(category);
        const char* name = getCategoryName(category);

        ss << color << std::left;
        ss << std::setw(ProfilerReport::COL_NAME) << name
            << std::setw(ProfilerReport::COL_TOTAL) << formatTimeCell(totalTime);

        double percentage = (totalTime / frameTotalTime) * 100.0;
        ss << std::setprecision(1) << percentage << "%";

        ss << ProfilerColors::RESET << "\n";
    }
}

void Profiler::printProfileReport()
{
    std::ostringstream ss;

    ss << "\n===[PERFORMANCE PROFILE REPORT]" << std::string(ProfilerReport::TOTAL_WIDTH - 32, '=') << "\n";
    ss << std::fixed << std::setprecision(4);

    printTableHeader(ss);

    auto sortedData = getAllProfileData();

    robin_hood::unordered_flat_map<ProfileCategory, double> categoryTotals;
    double totalTime = 0.0;

    for (const auto& pair : sortedData)
    {
        const auto& data = pair.second;
        categoryTotals[data.category] += data.totalTime;
        totalTime += data.totalTime;
    }

    if (totalTime > 0.0)
    {
        ss << "Entries Statistics:\n" << std::setprecision(1);
        for (const auto& pair : sortedData)
        {
            printProfileEntry(ss, pair.first, pair.second, totalTime);
        }

        ss << std::string(ProfilerReport::TOTAL_WIDTH, '=') << "\n";

        printCategoryStatistics(ss, categoryTotals, totalTime);
    }

    ss << std::string(ProfilerReport::TOTAL_WIDTH, '=') << "\n";

    std::cout << ss.str();

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