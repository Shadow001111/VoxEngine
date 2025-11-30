#pragma once
#include <iostream>
#include <sstream>
#include <cstdlib>

#define ENABLE_ASSERTS 1

#if defined(_MSC_VER)
#include <intrin.h>
#define DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define DEBUG_BREAK() __builtin_trap()
#else
#define DEBUG_BREAK() std::abort()
#endif

#if ENABLE_ASSERTS
#define ASSERT(expr)                                                             \
    do {                                                                         \
        if (!(expr)) {                                                           \
            std::ostringstream __ASSERT_stream;                                  \
            __ASSERT_stream << "Assertion failed!\n"                             \
                            << "Expression: " << #expr << "\n"                   \
                            << "File: " << __FILE__ << "\n"                      \
                            << "Line: " << __LINE__ << std::endl;                \
            std::cerr << __ASSERT_stream.str();                                  \
            DEBUG_BREAK();                                                       \
        }                                                                        \
    } while (0)
#else
#define ASSERT(expr)  (void)0
#endif