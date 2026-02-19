#pragma once
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <bit>
#include <algorithm>

#define TEST_UNSAFE false

#if TEST_UNSAFE
#include "Core/Assert.h"
#endif

template<typename T>
class ChunkSpecializedQueue
{
    using index_t = uint16_t;
    static constexpr index_t DEFAULT_CAPACITY = 1024;

    T* mData = nullptr;
    index_t mSize = 0;
    index_t mCapacity = 0;
    index_t mFrontIndex = 0;

    // Type requirements - must be trivially copyable and trivially destructible
    static_assert(std::is_trivially_copyable_v<T>,
        "ChunkSpecializedQueue only supports trivially copyable types");
    static_assert(std::is_trivially_destructible_v<T>,
        "ChunkSpecializedQueue only supports trivially destructible types");

    void grow()
    {
        if (mCapacity > std::numeric_limits<index_t>::max() / 2)
        {
            throw std::overflow_error("Cannot grow queue beyond maximum capacity");
        }
        reserve(mCapacity << 1);
    }

    static index_t round_up_to_power_of_two(index_t n)
    {
        if (n == 0) return 1;
        return static_cast<index_t>(1) << (std::bit_width(static_cast<size_t>(n)) - 1);
    }

    void reallocate_to_new_capacity(index_t new_capacity)
    {
        if (new_capacity <= mCapacity) return;

        T* new_data = static_cast<T*>(::operator new(new_capacity * sizeof(T)));

        if (mSize > 0)
        {
            if (mFrontIndex + mSize <= mCapacity)
            {
                std::memcpy(new_data, mData + mFrontIndex, mSize * sizeof(T));
            }
            else
            {
                index_t first_part = mCapacity - mFrontIndex;
                std::memcpy(new_data, mData + mFrontIndex, first_part * sizeof(T));
                std::memcpy(new_data + first_part, mData, (mSize - first_part) * sizeof(T));
            }
        }

        ::operator delete(mData);
        mData = new_data;
        mCapacity = new_capacity;
        mFrontIndex = 0;
    }

public:
    explicit ChunkSpecializedQueue(index_t initial_capacity = DEFAULT_CAPACITY)
    {
        index_t rounded_capacity = round_up_to_power_of_two(initial_capacity);
        mCapacity = std::max<index_t>(rounded_capacity, DEFAULT_CAPACITY);
        mData = static_cast<T*>(::operator new(mCapacity * sizeof(T)));
    }

    ~ChunkSpecializedQueue()
    {
        ::operator delete(mData);
    }

    ChunkSpecializedQueue(ChunkSpecializedQueue&& other) noexcept
        : mData(other.mData),
        mCapacity(other.mCapacity),
        mFrontIndex(other.mFrontIndex),
        mSize(other.mSize)
    {
        other.mData = nullptr;
        other.mCapacity = 0;
        other.mFrontIndex = 0;
        other.mSize = 0;
    }

    ChunkSpecializedQueue& operator=(ChunkSpecializedQueue&& other) noexcept
    {
        if (this != &other)
        {
            ::operator delete(mData);

            mData = other.mData;
            mCapacity = other.mCapacity;
            mFrontIndex = other.mFrontIndex;
            mSize = other.mSize;

            other.mData = nullptr;
            other.mCapacity = 0;
            other.mFrontIndex = 0;
            other.mSize = 0;
        }
        return *this;
    }

    ChunkSpecializedQueue(const ChunkSpecializedQueue&) = delete;
    ChunkSpecializedQueue& operator=(const ChunkSpecializedQueue&) = delete;

    void swap(ChunkSpecializedQueue& other) noexcept
    {
        using std::swap;
        swap(mData, other.mData);
        swap(mSize, other.mSize);
        swap(mCapacity, other.mCapacity);
        swap(mFrontIndex, other.mFrontIndex);
    }

    friend void swap(ChunkSpecializedQueue& lhs, ChunkSpecializedQueue& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return mSize == 0;
    }

    [[nodiscard]] index_t size() const noexcept
    {
        return mSize;
    }

    [[nodiscard]] index_t capacity() const noexcept
    {
        return mCapacity;
    }

    T& front()
    {
        if (empty())
        {
            throw std::out_of_range("Queue is empty");
        }
        return mData[mFrontIndex];
    }

    const T& front() const
    {
        if (empty())
        {
            throw std::out_of_range("Queue is empty");
        }
        return mData[mFrontIndex];
    }

    void push(const T& value)
    {
        if (mSize >= mCapacity)
        {
            grow();
        }
        index_t new_idx = (mFrontIndex + mSize) & (mCapacity - 1);
        mData[new_idx] = value;
        mSize++;
    }

    void push(T&& value)
    {
        if (mSize >= mCapacity)
        {
            grow();
        }
        index_t new_idx = (mFrontIndex + mSize) & (mCapacity - 1);
        mData[new_idx] = std::move(value);
        mSize++;
    }

    template<typename... Args>
    T& emplace(Args&&... args)
    {
        if (mSize >= mCapacity)
        {
            grow();
        }
        index_t new_idx = (mFrontIndex + mSize) & (mCapacity - 1);
        mData[new_idx] = T(std::forward<Args>(args)...);
        mSize++;
        return mData[new_idx];
    }

    void pop()
    {
        if (empty())
        {
            throw std::out_of_range("Queue is empty");
        }

        mFrontIndex = (mFrontIndex + 1) & (mCapacity - 1);
        mSize--;
    }

    void clear() noexcept
    {
        mFrontIndex = 0;
        mSize = 0;
    }

    void reserve(index_t new_capacity)
    {
        if (new_capacity > mCapacity)
        {
            index_t rounded_capacity = round_up_to_power_of_two(new_capacity);
            reallocate_to_new_capacity(rounded_capacity);
        }
    }

    // === UNSAFE METHODS ===
    // Caller must ensure the queue is not empty

    T& front_unsafe() noexcept
    {
#if TEST_UNSAFE
        ASSERT(!empty());
#endif
        return mData[mFrontIndex];
    }

    const T& front_unsafe() const noexcept
    {
#if TEST_UNSAFE
        ASSERT(!empty());
#endif
        return mData[mFrontIndex];
    }

    void pop_unsafe() noexcept
    {
#if TEST_UNSAFE
        ASSERT(!empty());
#endif
        mFrontIndex = (mFrontIndex + 1) & (mCapacity - 1);
        mSize--;
    }

    T pop_and_return_unsafe() noexcept
    {
#if TEST_UNSAFE
        ASSERT(!empty());
#endif
        T& result = mData[mFrontIndex];
        mFrontIndex = (mFrontIndex + 1) & (mCapacity - 1);
        mSize--;
        return result;
    }
};

#undef TEST_UNSAFE