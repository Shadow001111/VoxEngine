#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>

#include "Core/Container/Queue.h"

class ThreadPool
{
    using Task = std::move_only_function<void()>;
    using TaskQueue = Queue<Task>;

    DynamicArray<std::thread> workers;
    TaskQueue tasks;
	std::mutex queueMutex;
	std::condition_variable condition;
    std::atomic<bool> stop{ false };

    // Statistics
    std::atomic<uint64_t> taskTotalCount{ 0 };
public:
	ThreadPool(size_t numThreads = 0);
	~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<class F, class... Args>
    void enqueue(F&& f, Args&&... args);

	template<class F, class... Args>
    auto enqueueFuture(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>;

    void shutdown();

	//void waitForCompletion();
    size_t getThreadCount() const { return workers.size(); };
    size_t getTaskCount() const { return tasks.size(); }
    uint64_t getTaskTotalCount() const { return taskTotalCount.load(std::memory_order_relaxed); }
private:
	void workerThread();
};

template<class F, class ...Args>
inline void ThreadPool::enqueue(F&& f, Args && ...args)
{
    // If the pool is stopped, reject the task
    if (stop.load(std::memory_order_relaxed))
    {
        throw std::runtime_error("enqueue on stopped ThreadPool");
    }

    auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

    auto task = [bound = std::move(bound)]() mutable { bound(); };

    // Push the task into the queue
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.push(std::move(task));
    }
    condition.notify_one();
    taskTotalCount.fetch_add(1, std::memory_order_relaxed);
}

template<class F, class ...Args>
inline auto ThreadPool::enqueueFuture(F&& f, Args && ...args) -> std::future<typename std::invoke_result_t<F, Args...>>
{
    // If the pool is stopped, reject the task
    if (stop.load(std::memory_order_relaxed))
    {
        throw std::runtime_error("enqueue on stopped ThreadPool");
    }

    using return_type = typename std::invoke_result_t<F, Args...>;

    auto task = std::packaged_task<return_type()>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task.get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.emplace([task = std::move(task)]() mutable { task(); });
    }
    condition.notify_one();
    taskTotalCount.fetch_add(1, std::memory_order_relaxed);
    return res;
}

// Parallel execution utilities
class ParallelUtils
{
public:
    static ThreadPool& getGlobalThreadPool();

    template<typename Func>
    static void parallelFor(size_t start, size_t end, size_t minChunkSize, Func func);

    template<typename Container, typename Func>
    static void parallelForEach(Container& container, size_t minChunkSize, Func func);

    template<typename T, typename Func>
    static void parallelForEachInRange(T* array, size_t range, size_t minChunkSize, Func func);
private:
    static size_t calculateOptimalChunkCount(size_t totalItems, size_t minChunkSize);
};

template<typename Func>
void ParallelUtils::parallelFor(size_t start, size_t end, size_t minChunkSize, Func func)
{
    if (end <= start) return;

    const size_t totalItems = end - start;
    if (totalItems <= minChunkSize)
    {
        // Too small for parallelization
        for (size_t i = start; i < end; i++)
        {
            func(i);
        }
        return;
    }

    ThreadPool& pool = getGlobalThreadPool();
    size_t numThreads = pool.getThreadCount();
    size_t chunkSize = std::max(minChunkSize, (totalItems + numThreads - 1) / numThreads);
    
    size_t chunks = (totalItems + chunkSize - 1) / chunkSize;
    chunkSize = (totalItems + chunks - 1) / chunks;

    std::vector<std::future<void>> futures;
    futures.reserve(chunks);

    for (size_t i = start; i < end; i += chunkSize)
    {
        size_t chunkEnd = std::min(i + chunkSize, end);
        futures.emplace_back(pool.enqueueFuture([i, chunkEnd, func]()
            {
            for (size_t j = i; j < chunkEnd; j++)
            {
                func(j);
            }
            }));
    }

    // Wait for all chunks to complete
    for (auto& future : futures)
    {
        future.wait();
    }
}

template<typename Container, typename Func>
void ParallelUtils::parallelForEach(Container& container, size_t minChunkSize, Func func)
{
    parallelFor(0, container.size(), minChunkSize, [&container, func](size_t i)
        {
        func(container[i]);
        });
}

template<typename T, typename Func>
void ParallelUtils::parallelForEachInRange(T* array, size_t range, size_t minChunkSize, Func func)
{
    parallelFor(0, range, minChunkSize, [&array, func](size_t i)
        {
            func(array[i]);
        });
}