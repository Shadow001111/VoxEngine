#pragma once
#include <cstdint>

struct UnalignedBlockFace
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

    // Data5-10 (32 * 6 bits)
    uint32_t light0 : 8;
    uint32_t light1 : 8;
    uint32_t light2 : 8;
    uint32_t light3 : 8;

    uint32_t light4 : 8;
    uint32_t light5 : 8;
    uint32_t light6 : 8;
    uint32_t light7 : 8;

    uint32_t light8 : 8;
    uint32_t light9 : 8;
    uint32_t light10 : 8;
    uint32_t light11 : 8;

    uint32_t light12 : 8;
    uint32_t light13 : 8;
    uint32_t light14 : 8;
    uint32_t light15 : 8;

    uint32_t light16 : 8;
    uint32_t light17 : 8;
    uint32_t light18 : 8;
    uint32_t light19 : 8;

    uint32_t light20 : 8;
    uint32_t light21 : 8;
    uint32_t light22 : 8;
    uint32_t light23 : 8;

    // Data 11-12 (48 bits)
    uint32_t ao0 : 2;
    uint32_t ao1 : 2;
    uint32_t ao2 : 2;
    uint32_t ao3 : 2;
             
    uint32_t ao4 : 2;
    uint32_t ao5 : 2;
    uint32_t ao6 : 2;
    uint32_t ao7 : 2;
             
    uint32_t ao8 : 2;
    uint32_t ao9 : 2;
    uint32_t ao10 : 2;
    uint32_t ao11 : 2;
             
    uint32_t ao12 : 2;
    uint32_t ao13 : 2;
    uint32_t ao14 : 2;
    uint32_t ao15 : 2;
             
    uint32_t ao16 : 2;
    uint32_t ao17 : 2;
    uint32_t ao18 : 2;
    uint32_t ao19 : 2;
             
    uint32_t ao20 : 2;
    uint32_t ao21 : 2;
    uint32_t ao22 : 2;
    uint32_t ao23 : 2;
    // 16 bits left

    UnalignedBlockFace() = default;
};
