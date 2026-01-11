#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>

class ThreadPool
{
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks; // TODO: Maybe use something better, like vector as cicular queue
	std::mutex queueMutex;
	std::condition_variable condition;
	std::atomic<bool> stop;
public:
	ThreadPool(size_t numThreads = 0);
	~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
		-> std::future<typename std::invoke_result<F, Args...>::type>;

    template<class F, class... Args>
    auto broadcast(F&& f, Args&&... args);

    void shutdown();

	void waitForCompletion();
    size_t getThreadCount() const;
private:
	void workerThread();
};

template<class F, class ...Args>
inline auto ThreadPool::enqueue(F&& f, Args && ...args) -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    if (stop.load(std::memory_order_relaxed))
    {
        return res;
    }
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
}

template<class F, class... Args>
inline auto ThreadPool::broadcast(F&& f, Args&&... args)
{
    std::vector<std::future<void>> futures;

    if (stop.load(std::memory_order_relaxed))
    {
        return futures;
    }

    std::unique_lock<std::mutex> lock(queueMutex);

    for (size_t i = 0; i < workers.size(); ++i)
    {
        auto task = std::make_shared<std::packaged_task<void()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        futures.emplace_back(task->get_future());
        tasks.emplace([task]() { (*task)(); });
    }

    lock.unlock();
    condition.notify_all(); // wake all threads to grab the tasks
    return futures;
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
private:
    static size_t calculateOptimalChunkCount(size_t totalItems, size_t minChunkSize);
};

template<typename Func>
void ParallelUtils::parallelFor(size_t start, size_t end, size_t minChunkSize, Func func)
{
    if (end <= start) return;

    size_t totalItems = end - start;
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
        futures.emplace_back(pool.enqueue([i, chunkEnd, func]()
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