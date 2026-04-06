#pragma once
#include <cstdint>
#include "Core/Container/DynamicArray.h"

struct BlockModelData
{
    struct AlignedFace
    {
        uint8_t normal : 3 = 0;
        uint8_t textureSlot = 0;
    };

    struct UnalignedFace
    {
        uint32_t u0 : 5;
        uint32_t u1 : 5;
        uint32_t u2 : 5;
        uint32_t u3 : 5;

        uint32_t x0 : 5;
        uint32_t y0 : 5;
        uint32_t z0 : 5;
        uint32_t x1 : 5;
        uint32_t y1 : 5;
        uint32_t z1 : 5;

        uint32_t x2 : 5;
        uint32_t y2 : 5;
        uint32_t z2 : 5;
        uint32_t x3 : 5;
        uint32_t y3 : 5;
        uint32_t z3 : 5;

        uint32_t v0 : 5;
        uint32_t v1 : 5;
        uint32_t v2 : 5;
        uint32_t v3 : 5;
        uint32_t textureSlot : 12;
    };

    //std::string stringId;
    DynamicArray<AlignedFace> alignedFaces;
    DynamicArray<UnalignedFace> unalignedFaces;
};

