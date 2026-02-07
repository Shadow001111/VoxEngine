#pragma once

constexpr int CHUNK_SIZE_LOG2 = 4;
constexpr int CHUNK_SIZE = 1 << CHUNK_SIZE_LOG2;
constexpr int CHUNK_AREA = CHUNK_SIZE * CHUNK_SIZE;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
constexpr int CHUNK_LOWER_BITS_MASK = CHUNK_SIZE - 1;
constexpr int CHUNK_UPPER_BITS_MASK = ~CHUNK_LOWER_BITS_MASK;

constexpr int CHUNK_REGION_SIZE_LOG2 = 2; // The most optimal value. Other values result in more time spent on collecting chunks for rendering.
constexpr int CHUNK_REGION_SIZE = 1 << CHUNK_REGION_SIZE_LOG2;
constexpr int CHUNK_REGION_VOLUME = CHUNK_REGION_SIZE * CHUNK_REGION_SIZE * CHUNK_REGION_SIZE;
constexpr int CHUNK_REGION_LOWER_BITS_MASK = CHUNK_REGION_SIZE - 1;