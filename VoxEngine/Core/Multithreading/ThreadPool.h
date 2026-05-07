#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <queue>

#include "Core/Container/DynamicArray.h"
#include "Core/TracyProfiler.h"

#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
#include <functional>
template <class F>
using move_only_function_impl = std::move_only_function<F>;
#else
#include "move_only_function.h"
template <class F>
using move_only_function_impl = move_only_function<F>;
#endif

class ThreadPool
{
    using Task = move_only_function_impl<void()>;
    using TaskQueue = std::queue<Task>;// Queue<Task>;

    DynamicArray<std::thread> workers;
    TaskQueue tasks;
    TracyLockable(std::mutex, queueMutex);
	std::condition_variable_any newTaskCondition;
    std::condition_variable_any completionCondition;
    std::atomic<bool> stop{ false };
    std::atomic<size_t> activeTaskCount{ 0 };

    // Statistics
    std::atomic<size_t> taskTotalCount{ 0 };
public:
	ThreadPool(int numThreads = 0);
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
    size_t getTaskTotalCount() const { return taskTotalCount.load(std::memory_order_relaxed); }
private:
	void workerThread(int id);
};

template<class F, class ...Args>
inline void ThreadPool::enqueue(F&& f, Args && ...args)
{
	TRACY_SCOPE_N("ThreadPool::enqueue");

    // If the pool is stopped, reject the task
    if (stop.load(std::memory_order_relaxed))
    {
        throw std::runtime_error("enqueue on stopped ThreadPool");
    }

    auto task = [f = std::forward<F>(f),
        ...args = std::forward<Args>(args)]() mutable {
        std::invoke(std::move(f), std::move(args)...);
        };

    // Push the task into the queue
    {
        std::lock_guard lock(queueMutex);
        tasks.push(std::move(task));
    }
    newTaskCondition.notify_one();
    taskTotalCount.fetch_add(1, std::memory_order_relaxed);
}

template<class F, class ...Args>
inline auto ThreadPool::enqueueFuture(F&& f, Args && ...args) -> std::future<typename std::invoke_result_t<F, Args...>>
{
    TRACY_SCOPE_N("ThreadPool::enqueueFuture");

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
        std::lock_guard lock(queueMutex);
        tasks.emplace([task = std::move(task)]() mutable { task(); });
    }
    newTaskCondition.notify_one();
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
    TRACY_SCOPE_N("ThreadPool::parallelFor");

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

    {
		TRACY_SCOPE_N("Enqueue chunks");
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