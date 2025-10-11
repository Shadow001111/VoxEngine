#include "ProcessingFence.h"

void ProcessingFence::startProcessing()
{
    std::unique_lock<std::mutex> lock(mtx);
    // Wait until not processing
    cv.wait(lock, [&] { return !processing.load(std::memory_order_acquire); });
    // Mark as processing
    processing.store(true, std::memory_order_release);
}

void ProcessingFence::stopProcessing()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        processing.store(false, std::memory_order_release);
    }
    // Notify waiting threads
    cv.notify_one();
}

bool ProcessingFence::isProcessing() const noexcept
{
    return processing.load(std::memory_order_acquire);
}


ScopedProcessingFence::ScopedProcessingFence(ProcessingFence& fence) :
    fence(fence)
{
    fence.startProcessing();
}

ScopedProcessingFence::~ScopedProcessingFence()
{
    fence.stopProcessing();
}
