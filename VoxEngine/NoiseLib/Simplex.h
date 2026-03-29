#pragma once
#include "Base.h"

namespace NoiseLib::Simplex
{
    float scalar2D(const glm::vec2& p, int seed);
    float scalar3D(const glm::vec3& p, int seed);
    SimdF simd2D(const SimdF& vpx, const SimdF& vpy, const SimdI& vseed);
    SimdF simd3D(const SimdF& vpx, const SimdF& vpy, const SimdF& vpz, const SimdI& vseed);
}