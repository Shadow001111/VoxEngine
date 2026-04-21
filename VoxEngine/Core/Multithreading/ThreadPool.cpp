#include "ThreadPool.h"

#include "FileLogger.h"

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
        workers.emplace_back(&ThreadPool::workerThread, this, i);
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

void ThreadPool::workerThread(int id)
{
    {
        std::string threadName = std::to_string(id);
        tracy::SetThreadName(threadName.c_str());
    }

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
			logger.add("Warning: Invalid task encountered in thread pool worker thread");
            
            completionCondition.notify_all();
            continue; // Skip invalid tasks
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
