#include "NvidiaGPUUsage.h"
#include <memory>
#include <windows.h>

NvidiaGPUUsage::NvidiaGPUUsage() : processId(GetCurrentProcessId())
{
    LoadNVML();

    if (nvmlAvailable)
    {
        nvmlInit_v2();
        nvmlDeviceGetHandleByIndex_v2(0, &device);
    }
}

NvidiaGPUUsage::~NvidiaGPUUsage()
{
    if (nvmlAvailable && nvmlShutdown)
    {
        nvmlShutdown();
    }
    nvmlAvailable = false;
    if (nvmlModule)
    {
        FreeLibrary(static_cast<HMODULE>(nvmlModule));
    }
}

std::optional<float> NvidiaGPUUsage::getOverallGPUUtilization()
{
    if (!nvmlAvailable) return std::nullopt;

    nvmlUtilization_st utilization = {};
    nvmlDeviceGetUtilizationRates(device, &utilization);
    return static_cast<float>(utilization.gpu);
}

std::optional<unsigned long long> NvidiaGPUUsage::getProcessGPUMemory()
{
    if (!nvmlAvailable) return std::nullopt;

    unsigned int infoCount = 0;
    nvmlDeviceGetGraphicsRunningProcesses(device, &infoCount, nullptr);

    if (infoCount > 0)
    {
        std::unique_ptr<nvmlProcessInfo_st[]> processInfos(
            new nvmlProcessInfo_st[infoCount]);

        nvmlDeviceGetGraphicsRunningProcesses(device, &infoCount, processInfos.get());

        for (unsigned int i = 0; i < infoCount; i++)
        {
            if (processInfos[i].pid == processId)
            {
                return processInfos[i].usedGpuMemory;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> NvidiaGPUUsage::getDriverVersion()
{
    if (!nvmlAvailable) return std::nullopt;

    char version[80];
    nvmlSystemGetDriverVersion(version, sizeof(version));
    return std::string(version);
}

void NvidiaGPUUsage::LoadNVML()
{
    const char* possiblePaths[] = {
        "nvml.dll",
        "C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll",
        "C:\\Windows\\System32\\nvml.dll"
    };

    for (const char* path : possiblePaths)
    {
        nvmlModule = LoadLibraryA(path);
        if (nvmlModule) break;
    }

    if (!nvmlModule)
    {
        nvmlAvailable = false;
        return;
    }

    auto* module_ = static_cast<HMODULE>(nvmlModule);

    nvmlInit_v2 = (nvmlInit_v2_t)GetProcAddress(module_, "nvmlInit_v2");

    nvmlShutdown = (nvmlShutdown_t)GetProcAddress(module_, "nvmlShutdown");

    nvmlDeviceGetHandleByIndex_v2 = (nvmlDeviceGetHandleByIndex_v2_t)GetProcAddress(
        module_, "nvmlDeviceGetHandleByIndex_v2");

    nvmlDeviceGetUtilizationRates = (nvmlDeviceGetUtilizationRates_t)GetProcAddress(
        module_, "nvmlDeviceGetUtilizationRates");

    nvmlDeviceGetGraphicsRunningProcesses = (nvmlDeviceGetGraphicsRunningProcesses_t)GetProcAddress(
        module_, "nvmlDeviceGetGraphicsRunningProcesses");

    nvmlSystemGetDriverVersion = (nvmlSystemGetDriverVersion_t)GetProcAddress(
        module_, "nvmlSystemGetDriverVersion");

    nvmlAvailable = nvmlInit_v2 && nvmlShutdown &&
        nvmlDeviceGetHandleByIndex_v2 &&
        nvmlDeviceGetUtilizationRates;
}