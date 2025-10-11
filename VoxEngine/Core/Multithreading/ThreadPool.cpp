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
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();

    for (std::thread& worker : workers)
    {
        worker.join();
    }
}

void ThreadPool::waitForCompletion()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (tasks.empty())
            {
                break;
            }
        }
        std::this_thread::yield();
    }
}

size_t ThreadPool::getThreadCount() const
{
    return workers.size();
}

void ThreadPool::workerThread()
{
    while(true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });

            if (stop && tasks.empty())
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
