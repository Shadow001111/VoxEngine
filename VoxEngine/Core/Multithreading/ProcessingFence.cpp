#include "ProcessingFence.h"

void MutexCVFence::startProcessing()
{
    std::unique_lock<std::mutex> lock(mtx);
    // Wait until not processing
    cv.wait(lock, [&] { return !processing; });
    // Mark as processing
    processing = true;
}

void MutexCVFence::stopProcessing()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        processing = false;
    }
    // Notify waiting threads
	cv.notify_one(); // Allow one waiting thread to proceed
}


void AtomicWaitFence::startProcessing()
{
    bool expected = false;
    // Try to flip false -> true; if it fails, sleep until value changes.
    while (!processing.compare_exchange_weak(
        expected, true,
        std::memory_order_acquire,
        std::memory_order_relaxed))
    {
        processing.wait(true, std::memory_order_relaxed);
        expected = false;
    }
}

void AtomicWaitFence::stopProcessing()
{
    processing.store(false, std::memory_order_release);
    processing.notify_one();
}


void SemaphoreFence::startProcessing()
{
    sem.acquire();
    processing.store(true, std::memory_order_release);
}

void SemaphoreFence::stopProcessing()
{
    processing.store(false, std::memory_order_release);
    sem.release();
}