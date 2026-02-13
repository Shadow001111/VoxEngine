#pragma once
#include <optional>
#include <string>

class NvidiaGPUUsage
{
private:
    // Function pointer types for NVML
    using nvmlInit_v2_t = int (*)();
    using nvmlShutdown_t = int (*)();
    using nvmlDeviceGetHandleByIndex_v2_t = int (*)(unsigned int, void**);
    using nvmlDeviceGetUtilizationRates_t = int (*)(void*, void*);
    using nvmlDeviceGetGraphicsRunningProcesses_t = int (*)(void*, unsigned int*, void*);
    using nvmlSystemGetDriverVersion_t = int (*)(char*, unsigned int);

    // DLL and function pointers
    void* nvmlModule = nullptr;
    nvmlInit_v2_t nvmlInit_v2 = nullptr;
    nvmlShutdown_t nvmlShutdown = nullptr;
    nvmlDeviceGetHandleByIndex_v2_t nvmlDeviceGetHandleByIndex_v2 = nullptr;
    nvmlDeviceGetUtilizationRates_t nvmlDeviceGetUtilizationRates = nullptr;
    nvmlDeviceGetGraphicsRunningProcesses_t nvmlDeviceGetGraphicsRunningProcesses = nullptr;
    nvmlSystemGetDriverVersion_t nvmlSystemGetDriverVersion = nullptr;

    void* device = nullptr;
    unsigned int processId;
    bool nvmlAvailable = false;

    // NVML structs
    struct nvmlUtilization_st
    {
        unsigned int gpu;
        unsigned int memory;
    };

    struct nvmlProcessInfo_st
    {
        unsigned int pid;
        unsigned long long usedGpuMemory;
    };

    void LoadNVML();
public:
    NvidiaGPUUsage();
    ~NvidiaGPUUsage();

    bool isNVMLAvailable() const { return nvmlAvailable; }
    std::optional<float> getOverallGPUUtilization();
    std::optional<unsigned long long> getProcessGPUMemory();
    std::optional<std::string> getDriverVersion();
};