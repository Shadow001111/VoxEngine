#pragma once
#include <cstdint>

constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_AREA = CHUNK_SIZE * CHUNK_SIZE;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
constexpr int CHUNK_LOWER_BITS_MASK = CHUNK_SIZE - 1;
constexpr int CHUNK_UPPER_BITS_MASK = ~CHUNK_LOWER_BITS_MASK;