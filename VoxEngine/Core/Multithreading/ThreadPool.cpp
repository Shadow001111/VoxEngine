#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t numThreads) : stop(false)
{
    if (numThreads == 0)
    {
        numThreads = std::max(1u, std::thread::hardware_concurrency());
    }

    for (size_t i = 0; i < numThreads; i++)
    {
        workers.emplace_back(&ThreadPool::workerThread, this);
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

void ThreadPool::shutdown()
{
    stop.store(true, std::memory_order_release);
    condition.notify_all();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        while (!tasks.empty())
        {
            tasks.pop();
        }
    }

    for (std::thread& worker : workers)
    {
        worker.join();
    }

    workers.clear();
}

void ThreadPool::waitForCompletion()
{
    std::unique_lock<std::mutex> lock(queueMutex);
    condition.wait(lock, [this] { return tasks.empty(); });
}

void ThreadPool::workerThread()
{
    while(true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });

            if (stop.load(std::memory_order_relaxed))
            {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}

ThreadPool& ParallelUtils::getGlobalThreadPool()
{
    static ThreadPool pool;
    return pool;
}

size_t ParallelUtils::calculateOptimalChunkCount(size_t totalItems, size_t minChunkSize)
{
    ThreadPool& pool = getGlobalThreadPool();
    size_t maxChunks = pool.getThreadCount() * 2; // Allow some load balancing
    size_t minChunks = (totalItems - 1) / minChunkSize + 1;
    return std::min(maxChunks, minChunks);
}
