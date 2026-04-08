#include "ProcessingFence.h"


AtomicWaitFence::~AtomicWaitFence()
{
    processing.store(false, std::memory_order_release);
    processing.notify_all();
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


SemaphoreFence::~SemaphoreFence()
{
    stopProcessing();
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