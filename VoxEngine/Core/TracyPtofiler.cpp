#include "TracyProfiler.h"

#if TRACY_ENABLE

void* operator new(size_t size)
{
    void* ptr = malloc(size);
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void* ptr) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

void* operator new[](size_t size)
{
    void* ptr = malloc(size);
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete[](void* ptr) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

#endif