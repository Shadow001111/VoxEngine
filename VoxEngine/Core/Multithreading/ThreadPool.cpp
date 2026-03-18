#include "ThreadPool.h"

#include "FileLogger.h"

#include <iostream>

ThreadPool::ThreadPool(unsigned int numThreads)
{
    if (numThreads == 0)
    {
        const unsigned int minThreads = 1;
        const unsigned int maxThreads = 999;

        numThreads = std::thread::hardware_concurrency();
        if (numThreads > 1)
        {
            numThreads = std::clamp(numThreads - 1, minThreads, maxThreads);
        }
    }

    workers.reserve(numThreads);
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
    {
        std::unique_lock<std::mutex> lock(queueMutex);

        completionCondition.wait(lock, [this]
            {
                return tasks.empty() &&
                    activeTaskCount.load(std::memory_order_relaxed) == 0;
            });
    }
    stop.store(true, std::memory_order_release);
    newTaskCondition.notify_all();

    for (std::thread& worker : workers)
    {
        worker.join();
    }

    workers.clear();
}

void ThreadPool::workerThread()
{
    Task task;
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            newTaskCondition.wait(lock, [this] { return !tasks.empty() || stop.load(std::memory_order_relaxed); });

            if (stop.load(std::memory_order_relaxed))
            {
                break;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }

        if (!task)
        {
            FileLogger logger("log/warnings.txt");
			logger.add("Warning: Empty task encountered in thread pool worker thread");
            
            completionCondition.notify_all();
            continue; // Skip empty tasks
		}

        activeTaskCount.fetch_add(1, std::memory_order_relaxed);
        try
        {
            task();
        }
        catch (const std::exception& e)
        {
            FileLogger logger("log/warnings.txt");
			logger.add(std::string("Exception in thread pool worker thread: ") + e.what());
        }
        catch (...)
        {
			FileLogger logger("log/warnings.txt");
			logger.add("Unknown exception in thread pool worker thread");
        }
        activeTaskCount.fetch_sub(1, std::memory_order_relaxed);

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (tasks.empty() && activeTaskCount.load(std::memory_order_relaxed) == 0)
            {
                completionCondition.notify_all();
            }
        }
    }
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
