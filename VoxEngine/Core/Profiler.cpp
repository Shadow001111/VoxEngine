#include "Profiler.h"

#include <iostream>
#include <iomanip>
#include <algorithm>

void Profiler::ProfileData::addSample(double time) noexcept
{
    totalTime += time;
    if (time < minTime) minTime = time;
    if (time > maxTime) maxTime = time; // else if?
    callCount++;
}

void Profiler::ProfileData::reset() noexcept
{
    totalTime = 0.0;
    minTime = std::numeric_limits<double>::max();
    maxTime = 0.0;
    callCount = 0;
}


std::vector<Profiler::ThreadLocalData*> Profiler::threadRegistry;
std::mutex Profiler::threadRegistryMtx;
robin_hood::unordered_flat_map<Profiler::ProfileCategoryId, Profiler::ProfileCategoryData> Profiler::categoryRegistry;

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
    const char* RESET = "\033[0m";
}

std::string Profiler::make_ansi_prefix(Color color, Color bg, std::initializer_list<Style> styles)
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

const char* Profiler::getCategoryStyle(ProfileCategoryId category)
{
    auto it = categoryRegistry.find(category);
    if (it == categoryRegistry.end())
    {
        return "";
    }
    return it->second.style.c_str();
}

const char* Profiler::getCategoryName(ProfileCategoryId category)
{
    auto it = categoryRegistry.find(category);
    if (it == categoryRegistry.end())
    {
        return "Unknown";
    }
    return it->second.name.c_str();
}

std::vector<Profiler::NameData> Profiler::getMergeClearProfileData()
{
    ProfileDataMap merged;

    std::vector<ThreadLocalData*> threads;
    {
        std::lock_guard<std::mutex> lock(threadRegistryMtx);
        threads = threadRegistry;
    }

    for (ThreadLocalData* threadData : threads)
    {
        if (!threadData) continue;

        std::lock_guard<std::mutex> lock(threadData->mtx);

        for (const auto& [name, data] : threadData->profileData)
        {
            auto [it, inserted] = merged.emplace(name, data);
            if (!inserted)
            {
                ProfileData& dst = it->second;
                dst.totalTime += data.totalTime;
                dst.callCount += data.callCount;
                if (data.minTime < dst.minTime) dst.minTime = data.minTime;
                if (data.maxTime > dst.maxTime) dst.maxTime = data.maxTime;
                dst.category = data.category;
            }
        }

        threadData->profileData.clear();
    }

    std::vector<NameData> result;
    result.reserve(merged.size());

    for (auto& pair : merged)
    {
        result.emplace_back(pair.first, pair.second);
    }

    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b)
        {
            return a.second.totalTime > b.second.totalTime;
        });

    return result;
}

void Profiler::addSample(const char* name, double duration, ProfileCategoryId category)
{
    // Registers thread once
    thread_local ThreadLocalData threadLocalData;

    // Get profile data
    auto& profileData = threadLocalData.profileData;

    // Lock mutex
    std::lock_guard<std::mutex> lock(threadLocalData.mtx);

    // Add sample to the map
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

void Profiler::registerThread(ThreadLocalData* data)
{
    std::lock_guard<std::mutex> lock(threadRegistryMtx);
    threadRegistry.push_back(data);
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
    const char* color = getCategoryStyle(data.category);

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

void Profiler::printCategoryStatistics(std::ostringstream& ss, const robin_hood::unordered_flat_map<ProfileCategoryId, double>& categoryTotals, double frameTotalTime)
{
    if (categoryTotals.empty()) return;

    ss << "Category Statistics:\n";
    ss << std::left;
    ss << std::setw(ProfilerReport::COL_NAME) << "Category"
        << std::setw(ProfilerReport::COL_TOTAL) << "Total"
        << "% of Frame\n";
    ss << std::string(ProfilerReport::TOTAL_WIDTH, '-') << "\n";

    // Sort categories by total time (descending)
    std::vector<robin_hood::pair<ProfileCategoryId, double>> sortedCategories(categoryTotals.begin(), categoryTotals.end());
    std::sort(sortedCategories.begin(), sortedCategories.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& pair : sortedCategories)
    {
        ProfileCategoryId category = pair.first;
        double totalTime = pair.second;

        const char* color = getCategoryStyle(category);
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

    auto sortedData = getMergeClearProfileData();

    robin_hood::unordered_flat_map<ProfileCategoryId, double> categoryTotals;
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
}

Profiler::ThreadLocalData::ThreadLocalData()
{
    registerThread(this);
}
