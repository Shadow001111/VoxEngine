#pragma once
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>

class NothingFence
{
public:
    NothingFence() = default;
    ~NothingFence() = default;
    NothingFence(const NothingFence&) = delete;
    NothingFence& operator=(const NothingFence&) = delete;
    NothingFence(NothingFence&&) = delete;
    NothingFence& operator=(NothingFence&&) = delete;

    void startProcessing() {}
    void stopProcessing() {}

    // Note: This is not thread-safe. It should be used only for quick checks, where it's not critical if it returns a slightly outdated value.
    [[nodiscard]] bool isProcessing() const noexcept { return false; };
};

class MutexCVFence
{
    bool processing = false;
    std::mutex mtx;
    std::condition_variable cv;
public:
    MutexCVFence() = default;
	~MutexCVFence();
    MutexCVFence(const MutexCVFence&) = delete;
    MutexCVFence& operator=(const MutexCVFence&) = delete;
    MutexCVFence(MutexCVFence&&) = delete;
    MutexCVFence& operator=(MutexCVFence&&) = delete;

    void startProcessing();
    void stopProcessing();

    // Note: This is not thread-safe. It should be used only for quick checks, where it's not critical if it returns a slightly outdated value.
    [[nodiscard]] bool isProcessing() const noexcept { return processing; };
};

class AtomicWaitFence
{
    std::atomic<bool> processing = false;
public:
    AtomicWaitFence() = default;
    ~AtomicWaitFence();
    AtomicWaitFence(const AtomicWaitFence&) = delete;
    AtomicWaitFence& operator=(const AtomicWaitFence&) = delete;
    AtomicWaitFence(AtomicWaitFence&&) = delete;
    AtomicWaitFence& operator=(AtomicWaitFence&&) = delete;

    void startProcessing();
    void stopProcessing();

    [[nodiscard]] bool isProcessing() const noexcept { return processing.load(std::memory_order_acquire); };
};

class SemaphoreFence
{
    std::binary_semaphore sem{ 1 };   // 1 = available (not processing)
    std::atomic<bool> processing = false;
public:
    SemaphoreFence() = default;
    ~SemaphoreFence();
    SemaphoreFence(const SemaphoreFence&) = delete;
    SemaphoreFence& operator=(const SemaphoreFence&) = delete;
    SemaphoreFence(SemaphoreFence&&) = delete;
    SemaphoreFence& operator=(SemaphoreFence&&) = delete;

    void startProcessing();
    void stopProcessing();

    [[nodiscard]] bool isProcessing() const noexcept { return processing.load(std::memory_order_acquire); };
};

template <typename T>
class FenceGuard
{
    T& fence;
public:
    explicit FenceGuard(T& fence) :
        fence(fence)
    {
		fence.startProcessing();
    }

    ~FenceGuard()
    {
		fence.stopProcessing();
    }

    FenceGuard(const FenceGuard&) = delete;
    FenceGuard& operator=(const FenceGuard&) = delete;
    FenceGuard(FenceGuard&&) = delete;
    FenceGuard& operator=(FenceGuard&&) = delete;
};