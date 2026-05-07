#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>

#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
#include <functional>
template <class F>
using move_only_function_impl = std::move_only_function<F>;
#else
#include "move_only_function.h"
template <class F>
using move_only_function_impl = move_only_function<F>;
#endif

#include "Core/TracyProfiler.h"

class WorkerThread
{
public:
	using Task = move_only_function_impl<void()>;
	using TaskQueue = std::queue<Task>;

	struct Context
	{
		TaskQueue tasks;
		TracyLockableN(std::mutex, queueMutex, "ThreadPool queue mutex");
		std::condition_variable_any newTaskCondition;
		std::condition_variable_any completionCondition;
		std::atomic<bool> stop{ false };
		std::atomic<size_t> activeTaskCount{ 0 };
	};
private:
	std::thread thread;
	Context& context;
public:
	WorkerThread(uint32_t id, Context& ctx);

	void join();
private:
	void run();
};