#include <atomic>
#include <type_traits>
#include <cassert>

template <typename T>
    requires (std::is_unsigned_v<T>)
struct AtomicFlags
{
private:
    std::atomic<T> bits{ 0 };
public:
    void set(unsigned index, bool value) noexcept 
    {
        assert(index < sizeof(T) * 8);
        T mask = static_cast<T>(T(1) << index);
        if (value)
        {
            bits.fetch_or(mask, std::memory_order_acq_rel);
        }
        else
        {
            bits.fetch_and(~mask, std::memory_order_acq_rel);
        }
    }

    void reset() noexcept
    {
        bits.store(0, std::memory_order_release);
    }

    bool read(unsigned index) const noexcept
    {
        assert(index < sizeof(T) * 8);
        T mask = static_cast<T>(T(1) << index);
        return (bits.load(std::memory_order_acquire) & mask) != 0;
    }
};
