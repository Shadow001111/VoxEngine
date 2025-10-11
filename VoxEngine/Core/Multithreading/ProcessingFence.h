#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>

class ProcessingFence
{
    std::atomic<bool> processing{ false };
    std::mutex mtx;
    std::condition_variable cv;
public:
    void startProcessing();
    void stopProcessing();

    bool isProcessing() const noexcept;
};

class ScopedProcessingFence
{
    ProcessingFence& fence;
public:
    explicit ScopedProcessingFence(ProcessingFence& fence);
    ~ScopedProcessingFence();

    ScopedProcessingFence(const ScopedProcessingFence&) = delete;
    ScopedProcessingFence& operator=(const ScopedProcessingFence&) = delete;
    ScopedProcessingFence(ScopedProcessingFence&&) = delete;
    ScopedProcessingFence& operator=(ScopedProcessingFence&&) = delete;
};