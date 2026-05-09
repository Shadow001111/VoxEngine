#include "ThreadPool.h"

#include "Core/TracyProfiler.h"

ThreadPool::ThreadPool(int numThreads)
{
    if (numThreads == 0)
    {
        const int minThreads = 1;
        const int maxThreads = 999;

        numThreads = std::thread::hardware_concurrency();
        numThreads = std::clamp(numThreads - 1, minThreads, maxThreads);
    }

    workers.reserve(numThreads);
    for (size_t i = 0; i < numThreads; i++)
    {
        workers.emplace_back(i, context);
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

void ThreadPool::shutdown()
{
    {
        std::unique_lock lock(context.queueMutex);

        context.completionCondition.wait(lock, [this]
            {
                return context.tasks.empty() && context.activeTaskCount.load(std::memory_order_relaxed) == 0;
            });

        context.stop.store(true, std::memory_order_release);
    }
    context.newTaskCondition.notify_all();

    for (auto& worker : workers)
    {
        worker.join();
    }

    workers.clear();
}

ThreadPool& ParallelUtils::getGlobalThreadPool()
{
    static ThreadPool pool;
    return pool;
}

size_t ParallelUtils::calculateOptimalChunkCount(size_t totalItems, size_t minChunkSize)
{
    const ThreadPool& pool = getGlobalThreadPool();
    size_t maxChunks = pool.getThreadCount() * 2; // Allow some load balancing
    size_t minChunks = (totalItems - 1) / minChunkSize + 1;
    return std::min(maxChunks, minChunks);
}
