#pragma once
#include <cstdint>

// TODO: Pack data
struct NonAlignedBlockFace
{
    float x0, y0, z0;
    float x1, y1, z1;
    float x2, y2, z2;
    float x3, y3, z3;

    float u0, v0;
    float u1, v1;
    float u2, v2;
    float u3, v3;

    uint32_t textureID;

    // TODO: Add light and AO

    NonAlignedBlockFace() = default;
    //NonAlignedBlockFace(uint32_t x, uint32_t y, uint32_t z, uint32_t u, uint32_t v, uint32_t ao, uint32_t textureID, uint64_t light);
};
