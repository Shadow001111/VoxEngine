#pragma once
#include <iostream>
#include <vector>
#include <algorithm> // for std::swap

template <typename T, size_t N>
class SymmetricMatrix {
    T data[N * (N + 1) / 2];

    // Compute the 1D index for (i, j) in upper triangle
    constexpr size_t index(size_t i, size_t j) const
    {
        if (i > j) std::swap(i, j);
        return i * N - ((i * (i - 1)) >> 1) + (j - i);
    }

public:
    SymmetricMatrix() {}

    T& get(size_t i, size_t j) { return data[index(i, j)]; }
    const T& get(size_t i, size_t j) const { return data[index(i, j)]; }

    void fill(const T& value)
    {
        std::fill(data.begin(), data.end(), value);
    }

    void print(std::ostream& os = std::cout) const {
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j)
                os << at(i, j) << ' ';
            os << '\n';
        }
    }
};
