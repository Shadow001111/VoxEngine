#pragma once
#include "FixedArenaAllocator.h"

template<typename T, size_t ArenaBlockSize = 256>
class FixedArenaObjectPool
{
	FixedArenaAllocator<T, ArenaBlockSize> allocator;
protected:
	std::vector<T*> pool;
public:
    size_t additionalAllocationCount = 10;

    FixedArenaObjectPool() = default;
    ~FixedArenaObjectPool() = default;

    FixedArenaObjectPool(const FixedArenaObjectPool&) = delete;
    FixedArenaObjectPool& operator=(const FixedArenaObjectPool&) = delete;
    FixedArenaObjectPool(FixedArenaObjectPool&&) = delete;
    FixedArenaObjectPool& operator=(FixedArenaObjectPool&&) = delete;

    T* acquire()
    {
        // Check if there are available objects in pool
        if (!pool.empty())
        {
            T* object = pool.back();
            pool.pop_back();
            return object;
        }

        // When no objects in pool, allocate additional ones
        allocate(additionalAllocationCount);

        // And one more too
        return allocator.create();
    }

    void release(T* object)
    {
        pool.push_back(object);
    }

    void allocate(size_t count)
    {
        pool.reserve(pool.size() + count);
        for (size_t i = 0; i < count; i++)
        {
            pool.push_back(allocator.create());
        }
    }

    size_t getAvailableCount() const { return pool.size(); }
};

