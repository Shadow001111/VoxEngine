#pragma once
#include <cstdint>

struct NonAlignedBlockFace
{
    // Data1 (32 bits)
    uint32_t blockX : 4;
    uint32_t blockY : 4;
    uint32_t blockZ : 4;
    uint32_t u0 : 5;
    uint32_t u1 : 5;
    uint32_t u2 : 5;
    uint32_t u3 : 5;

    // Data2 (32 bits)
    uint32_t x0 : 5;
    uint32_t y0 : 5;
    uint32_t z0 : 5;
    uint32_t x1 : 5;
    uint32_t y1 : 5;
    uint32_t z1 : 5;
    // 2 bits left

    // Data3 (32 bits)
    uint32_t x2 : 5;
    uint32_t y2 : 5;
    uint32_t z2 : 5;
    uint32_t x3 : 5;
    uint32_t y3 : 5;
    uint32_t z3 : 5;
    // 2 bits left

    // Data4 (32 bits)
    uint32_t v0 : 5;
    uint32_t v1 : 5;
    uint32_t v2 : 5;
    uint32_t v3 : 5;
    uint32_t textureID : 12;

    // Data5 (32 bits)
    uint32_t light0 : 8;
    uint32_t light1 : 8;
    uint32_t light2 : 8;
    uint32_t light3 : 8;

    // Data6 (32 bits)
    uint32_t light4 : 8;
    uint32_t light5 : 8;
    uint32_t light6 : 8;
    uint32_t light7 : 8;

    // Data7 (32 bits)
    uint32_t ao0 : 2;
    uint32_t ao1 : 2;
    uint32_t ao2 : 2;
    uint32_t ao3 : 2;
    uint32_t ao4 : 2;
    uint32_t ao5 : 2;
    uint32_t ao6 : 2;
    uint32_t ao7 : 2;
    // 16 bits left
    // TODO: Maybe allocate 3 bits for ao there, since maybe I will make other AO model, with value range of [0; 7]

    NonAlignedBlockFace() = default;
};
