#include "WorkerThread.h"
#include "FileLogger.h"
#include <string>

WorkerThread::WorkerThread(uint32_t id, Context& ctx) :
	context(ctx)
{
	thread = std::thread([this, id]
		{
			std::string threadName = std::to_string(id);
			tracy::SetThreadName(threadName.c_str());
			run();
		});
}

void WorkerThread::run()
{
    Task task;
    while (true)
    {
        {
            std::unique_lock lock(context.queueMutex);
            context.newTaskCondition.wait(lock, [this] { return !context.tasks.empty() || context.stop.load(std::memory_order_relaxed); });

            if (context.stop.load(std::memory_order_relaxed))
            {
                break;
            }

            task = std::move(context.tasks.front());
            context.tasks.pop();
            context.activeTaskCount.fetch_add(1, std::memory_order_relaxed);
        }

        if (task) [[likely]]
        {
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
        }
        else
        {
            FileLogger logger("log/warnings.txt");
            logger.add("Warning: Invalid task encountered in thread pool worker thread");
        }

        const size_t oldTaskCount = context.activeTaskCount.fetch_sub(1, std::memory_order_relaxed);

        if (oldTaskCount == 1)
        {
            std::lock_guard lock(context.queueMutex);
            if (context.tasks.empty())
            {
                context.completionCondition.notify_all();
            }
        }
    }
}

void WorkerThread::join()
{
    if (thread.joinable())
    {
        thread.join();
    }
}