#include <atomic>
#include <mutex>
#include <condition_variable>

class ProcessingFence
{
    std::atomic<bool> processing{ false };
    std::mutex mtx;
    std::condition_variable cv;
public:
    void startProcessing();
    void stopProcessing();

    bool isProcessing() const noexcept;
};
