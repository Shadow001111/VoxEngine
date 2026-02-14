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

    T* data_ = nullptr;
    index_t capacity_ = 0;
    index_t front_idx_ = 0;
    index_t size_ = 0;

    // Type requirements - must be trivially copyable and trivially destructible
    static_assert(std::is_trivially_copyable_v<T>,
        "ChunkSpecializedQueue only supports trivially copyable types");
    static_assert(std::is_trivially_destructible_v<T>,
        "ChunkSpecializedQueue only supports trivially destructible types");

    void grow_if_needed()
    {
        if (size_ >= capacity_)
        {
            reserve(capacity_ << 1);
        }
    }

    static index_t round_up_to_power_of_two(index_t n)
    {
        if (n == 0) return 1;
        return static_cast<index_t>(1) << (std::bit_width(static_cast<size_t>(n)) - 1);
    }

    void reallocate_to_new_capacity(index_t new_capacity)
    {
        if (new_capacity <= capacity_) return;

        T* new_data = static_cast<T*>(::operator new(new_capacity * sizeof(T)));

        if (size_ > 0)
        {
            if (front_idx_ + size_ <= capacity_)
            {
                std::memcpy(new_data, data_ + front_idx_, size_ * sizeof(T));
            }
            else
            {
                index_t first_part = capacity_ - front_idx_;
                std::memcpy(new_data, data_ + front_idx_, first_part * sizeof(T));
                std::memcpy(new_data + first_part, data_, (size_ - first_part) * sizeof(T));
            }
        }

        ::operator delete(data_);
        data_ = new_data;
        capacity_ = new_capacity;
        front_idx_ = 0;
    }

public:
    explicit ChunkSpecializedQueue(index_t initial_capacity = DEFAULT_CAPACITY)
    {
        index_t rounded_capacity = round_up_to_power_of_two(initial_capacity);
        capacity_ = std::max<index_t>(rounded_capacity, DEFAULT_CAPACITY);
        data_ = static_cast<T*>(::operator new(capacity_ * sizeof(T)));
    }

    ~ChunkSpecializedQueue()
    {
        ::operator delete(data_);
    }

    ChunkSpecializedQueue(ChunkSpecializedQueue&& other) noexcept
        : data_(other.data_),
        capacity_(other.capacity_),
        front_idx_(other.front_idx_),
        size_(other.size_)
    {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.front_idx_ = 0;
        other.size_ = 0;
    }

    ChunkSpecializedQueue& operator=(ChunkSpecializedQueue&& other) noexcept
    {
        if (this != &other)
        {
            ::operator delete(data_);

            data_ = other.data_;
            capacity_ = other.capacity_;
            front_idx_ = other.front_idx_;
            size_ = other.size_;

            other.data_ = nullptr;
            other.capacity_ = 0;
            other.front_idx_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    ChunkSpecializedQueue(const ChunkSpecializedQueue&) = delete;
    ChunkSpecializedQueue& operator=(const ChunkSpecializedQueue&) = delete;

    void swap(ChunkSpecializedQueue& other) noexcept
    {
        using std::swap;
        swap(data_, other.data_);
        swap(capacity_, other.capacity_);
        swap(front_idx_, other.front_idx_);
        swap(size_, other.size_);
    }

    friend void swap(ChunkSpecializedQueue& lhs, ChunkSpecializedQueue& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size_ == 0;
    }

    [[nodiscard]] index_t size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] index_t capacity() const noexcept
    {
        return capacity_;
    }

    T& front()
    {
        if (empty())
        {
            throw std::out_of_range("Queue is empty");
        }
        return data_[front_idx_];
    }

    const T& front() const
    {
        if (empty())
        {
            throw std::out_of_range("Queue is empty");
        }
        return data_[front_idx_];
    }

    void push(const T& value)
    {
        grow_if_needed();
        index_t new_idx = (front_idx_ + size_) & (capacity_ - 1);
        data_[new_idx] = value;
        size_++;
    }

    void push(T&& value)
    {
        grow_if_needed();
        index_t new_idx = (front_idx_ + size_) & (capacity_ - 1);
        data_[new_idx] = std::move(value);
        size_++;
    }

    template<typename... Args>
    T& emplace(Args&&... args)
    {
        grow_if_needed();
        index_t new_idx = (front_idx_ + size_) & (capacity_ - 1);
        data_[new_idx] = T(std::forward<Args>(args)...);
        size_++;
        return data_[new_idx];
    }

    void pop()
    {
        if (empty())
        {
            throw std::out_of_range("Queue is empty");
        }

        front_idx_ = (front_idx_ + 1) & (capacity_ - 1);
        size_--;
    }

    void clear() noexcept
    {
        front_idx_ = 0;
        size_ = 0;
    }

    void reserve(index_t new_capacity)
    {
        if (new_capacity > capacity_)
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
        return data_[front_idx_];
    }

    const T& front_unsafe() const noexcept
    {
#if TEST_UNSAFE
        ASSERT(!empty());
#endif
        return data_[front_idx_];
    }

    void pop_unsafe() noexcept
    {
#if TEST_UNSAFE
        ASSERT(!empty());
#endif
        front_idx_ = (front_idx_ + 1) & (capacity_ - 1);
        size_--;
    }

    T pop_and_return_unsafe() noexcept
    {
#if TEST_UNSAFE
        ASSERT(!empty());
#endif
        T& result = data_[front_idx_];
        front_idx_ = (front_idx_ + 1) & (capacity_ - 1);
        size_--;
        return result;
    }
};

#undef TEST_UNSAFE