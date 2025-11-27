#pragma once
#include <cstdint>

// TODO: Add light and AO
struct NonAlignedBlockFace
{
    // Data1 (32 bits)
    uint32_t x0 : 9;
    uint32_t y0 : 9;
    uint32_t z0 : 9;
    uint32_t u0 : 5;

    // Data2 (32 bits)
    uint32_t x1 : 9;
    uint32_t y1 : 9;
    uint32_t z1 : 9;
    uint32_t u1 : 5;

    // Data3 (32 bits)
    uint32_t x2 : 9;
    uint32_t y2 : 9;
    uint32_t z2 : 9;
    uint32_t u2 : 5;

    // Data4 (32 bits)
    uint32_t x3 : 9;
    uint32_t y3 : 9;
    uint32_t z3 : 9;
    uint32_t u3 : 5;

    // Data5 (32 bits)
    uint32_t v0 : 5;
    uint32_t v1 : 5;
    uint32_t v2 : 5;
    uint32_t v3 : 5;
    uint32_t textureID : 12;

    NonAlignedBlockFace() = default;
};
