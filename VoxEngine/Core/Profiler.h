#pragma once
#include <chrono>
#include "robin_hood.h"
#include <vector>
#include <limits>
#include <mutex>
#include <sstream>
#include <cstring>
#include "Debug.h"

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
public:
    using ProfileCategoryId = uint64_t;

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
private:
    struct ProfileData
    {
        double totalTime = 0.0;
        double minTime = std::numeric_limits<double>::max();
        double maxTime = 0.0;
        uint64_t callCount = 0;
        ProfileCategoryId category = 0;

        double getAverageTime() const noexcept { return callCount > 0 ? totalTime / static_cast<double>(callCount) : 0.0; }
        void addSample(double time) noexcept;
        void reset() noexcept;
    };

    struct ProfileCategoryData
    {
        std::string name;
        std::string style;
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
    static robin_hood::unordered_flat_map<ProfileCategoryId, ProfileCategoryData> categoryRegistry;

    static std::string make_ansi_prefix(Color color = Color::Default, Color bg = Color::Default, std::initializer_list<Style> styles = {});

    static const char* getCategoryStyle(ProfileCategoryId category);
    static const char* getCategoryName(ProfileCategoryId category);

    static std::vector<NameData> getMergeClearProfileData();
public:
    static void registerThread(ThreadLocalData* data);

    template<typename T>
    static void registerProfileCategory(
        T category,
        const char* name,
        Color color = Color::Default, 
        Color bg = Color::Default, 
        std::initializer_list<Style> styles = {}
    )
    {
        ProfileCategoryId catId = static_cast<ProfileCategoryId>(category);

        auto& catData = categoryRegistry[catId];
        catData.name = std::string(name);
        catData.style = make_ansi_prefix(color, bg, styles);
    }

    static void addSample(const char* name, double duration, ProfileCategoryId category);

    static void printProfileReport();
private:
    static void printTableHeader(std::ostringstream& ss);
    static void printProfileEntry(std::ostringstream& ss, const char* name, const ProfileData& data, double frameTotalTime);
    static void printCategoryStatistics(std::ostringstream& ss, const robin_hood::unordered_flat_map<ProfileCategoryId, double>& categoryTotals, double frameTotalTime);
};

template<typename T>
class ScopedProfiler
{
private:
    using Clock = std::chrono::steady_clock;

    const char* name;
    Profiler::ProfileCategoryId category;
    Clock::time_point startTime;

public:
    ScopedProfiler(const char* profileName, T categoryValue) :
        name(profileName),
        category(static_cast<Profiler::ProfileCategoryId>(categoryValue)),
        startTime(Clock::now())
    {
    }

    ~ScopedProfiler()
    {
        const auto endTime = Clock::now();
        const double duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        Profiler::addSample(name, duration, category);
    }
};

#define PROFILE_SCOPE_CONCAT_INNER(a, b) a##b
#define PROFILE_SCOPE_CONCAT(a, b) PROFILE_SCOPE_CONCAT_INNER(a, b)

#if PROFILING_ENABLED
    #ifdef TRACY_ENABLE
        #include <tracy/Tracy.hpp>
        #define PROFILE_SCOPE(name, category) ZoneScopedN(name)
    #else
        #define PROFILE_SCOPE(name, category) ScopedProfiler PROFILE_SCOPE_CONCAT(_profiler_, __COUNTER__)(name, category)
    #endif
#else
    #define PROFILE_SCOPE(name, category)
#endif