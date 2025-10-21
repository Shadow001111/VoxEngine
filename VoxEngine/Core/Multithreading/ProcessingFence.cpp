#include "ProcessingFence.h"

void ProcessingFence::startProcessing()
{
    std::unique_lock<std::mutex> lock(mtx);
    // Wait until not processing
    cv.wait(lock, [&] { return !processing; });
    // Mark as processing
    processing = true;
}

void ProcessingFence::stopProcessing()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        processing = false;
    }
    // Notify waiting threads
    cv.notify_one();
}

bool ProcessingFence::isProcessing() const noexcept
{
    return processing;
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
