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

//struct NonAlignedBlockFace2
//{
//    // Data1 (32 bits)
//    uint32_t x0 : 8;
//    uint32_t y0 : 8;
//    uint32_t z0 : 8;
//    // 8 bits left
//
//    // Data2 (32 bits)
//    uint32_t dx1 : 5;
//    uint32_t dy1 : 5;
//    uint32_t dz1 : 5;
//    uint32_t dx2 : 5;
//    uint32_t dy2 : 5;
//    uint32_t dz2 : 5;
//    // 2 bits left
//
//    // Data3 (32 bits)
//    uint32_t dx3 : 5;
//    uint32_t dy3 : 5;
//    uint32_t dz3 : 5;
//
//    float u0, v0;
//    float u1, v1;
//    float u2, v2;
//    float u3, v3;
//
//    uint32_t textureID;
//
//    // TODO: Add light and AO
//
//    NonAlignedBlockFace2() = default;
//};
