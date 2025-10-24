#pragma once
#include <bitset>
#include <iostream>

template <size_t N>
class SymmetricBitMatrix
{
    static constexpr size_t SIZE = (N * (N + 1)) >> 1;
    std::bitset<SIZE> data;

    constexpr size_t index(size_t i, size_t j) const
    {
        if (i > j) std::swap(i, j);
        return i * N - (i * (i - 1)) / 2 + (j - i);
    }

public:
    bool get(size_t i, size_t j) const { return data.test(index(i, j)); }
    void set(size_t i, size_t j, bool val) { data.set(index(i, j), val); }

    void fill(bool value)
    {
        data.set();
        if (!value) data.reset();
    }

    void print(std::ostream& os = std::cout) const
    {
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j)
                os << get(i, j) << ' ';
            os << '\n';
        }
    }
};