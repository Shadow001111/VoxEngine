#pragma once
#include "Core/Simd.h"
#include <glm/glm.hpp>

namespace NoiseLib::Base
{
    static_assert(SimdF::lanes == 8 || SimdF::lanes == 4, "Unsupported lane count");

    float sumOfGeometricSeries(float firstTerm, float commonRatio, int numberOfTerms);


    __forceinline inline uint32_t hash_int2(const glm::ivec2& v)
    {
        uint32_t h = 0xdeadbeefu;
        h ^= (uint32_t)v.x + 0x9e3779b9u + (h << 6u) + (h >> 2u);
        h ^= (uint32_t)v.y + 0x9e3779b9u + (h << 6u) + (h >> 2u);
        h ^= h >> 16u;
        h *= 0x85ebca6bu;
        h ^= h >> 13u;
        h *= 0xc2b2ae35u;
        h ^= h >> 16u;
        return h;
    }

    __forceinline inline uint32_t hash_int3(const glm::ivec3& v)
    {
        uint32_t h = 0xdeadbeefu;
        h ^= (uint32_t)v.x + 0x9e3779b9u + (h << 6u) + (h >> 2u);
        h ^= (uint32_t)v.y + 0x9e3779b9u + (h << 6u) + (h >> 2u);
        h ^= (uint32_t)v.z + 0x9e3779b9u + (h << 6u) + (h >> 2u);
        h ^= h >> 16u;
        h *= 0x85ebca6bu;
        h ^= h >> 13u;
        h *= 0xc2b2ae35u;
        h ^= h >> 16u;
        return h;
    }

    __forceinline inline SimdI hash_int2_simd(const SimdI& vx, const SimdI& vy)
    {
        const SimdI MAGIC(0x9e3779b9);
        const SimdI M1(0x85ebca6b);
        const SimdI M2(0xc2b2ae35);

        SimdI h = SimdI(0xdeadbeef);

        SimdI t;

        t = vx + MAGIC;
        t += h << 6;
        t += h >> 2;
        h ^= t;

        t = vy + MAGIC;
        t += h << 6;
        t += h >> 2;
        h ^= t;

        // Murmur-style avalanche
        h ^= h >> 16;
        h *= M1;
        h ^= h >> 13;
        h *= M2;
        h ^= h >> 16;
        return h;
    }

    __forceinline inline SimdI hash_int3_simd(const SimdI& vx, const SimdI& vy, const SimdI& vz)
    {
        const SimdI MAGIC(0x9e3779b9);
        const SimdI M1(0x85ebca6b);
        const SimdI M2(0xc2b2ae35);

        SimdI h = SimdI(0xdeadbeef);

        SimdI t;

        t = vx + MAGIC;
        t += h << 6;
        t += h >> 2;
        h ^= t;

        t = vy + MAGIC;
        t += h << 6;
        t += h >> 2;
        h ^= t;

        t = vz + MAGIC;
        t += h << 6;
        t += h >> 2;
        h ^= t;

        // Murmur-style avalanche
        h ^= h >> 16;
        h *= M1;
        h ^= h >> 13;
        h *= M2;
        h ^= h >> 16;
        return h;
    }


    __forceinline inline int wrap(int idx, int period)
    {
        return idx - period * floorf((float)idx / (float)period);
    }

    __forceinline inline glm::ivec2 wrap(const glm::ivec2& idx, int period)
    {
        return glm::vec2(idx) - (float)period * glm::floor(glm::vec2(idx) / (float)period);
    }

    __forceinline inline glm::ivec3 wrap(const glm::ivec3& idx, int period)
    {
        return glm::vec3(idx) - (float)period * glm::floor(glm::vec3(idx) / (float)period);
    }

    __forceinline inline SimdI wrap_simd(const SimdI& idx, const SimdI& period)
    {
        auto idxF = idx.to_float();
        auto periodF = period.to_float();
        auto div = idxF / periodF;
        //return (idxF - periodF * SimdF::floor(div)).to_int32();
        return SimdF::neg_mul_add(periodF, SimdF::floor(div), idxF).to_int32();
    }


    __forceinline inline float grad2(uint32_t hash, const glm::vec2& p)
    {
        switch (hash & 7)
        {
        case 0: return  p.x + p.y;
        case 1: return -p.x + p.y;
        case 2: return  p.x - p.y;
        case 3: return -p.x - p.y;
        case 4: return  p.x;
        case 5: return -p.x;
        case 6: return  p.y;
        default:return -p.y;
        }
    }

    __forceinline inline float grad3(uint32_t hash, const glm::vec3& p)
    {
        constexpr glm::vec3 gradients[16] = {
            { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
            { 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
            { 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1},
            { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
        };
        return glm::dot(gradients[hash & 15u], p);
    }

    __forceinline inline SimdF grad2_simd(const SimdI& hash, const SimdF& dx, const SimdF& dy)
    {
        const SimdI b0(1);
        const SimdI b1(2);
        const SimdI b2(4);
        const SimdF sign(-0.0f);

        SimdF mbit0 = (((hash & b0) == b0)).as_float();
        SimdF mbit1 = (((hash & b1) == b1)).as_float();
        SimdF mbit2 = (((hash & b2) == b2)).as_float();

        SimdF sign0 = mbit0 & sign;
        SimdF sign1 = mbit1 & sign;

        SimdF gx = dx ^ sign0;
        SimdF gy = dy ^ sign1;
        SimdF both = gx + gy;

        SimdF single = SimdF::blendv(dx, dy, mbit1) ^ sign0;

        return SimdF::blendv(both, single, mbit2);
    }

    __forceinline inline SimdF grad3_simd(const SimdI& hash, const SimdF& dx, const SimdF& dy, const SimdF& dz)
    {
        const SimdI b0(1);
        const SimdI b1(2);
        const SimdI b2(4);
        const SimdI b3(8);
        const SimdF sign(-0.0f);

        SimdF mbit0 = (((hash & b0) == b0)).as_float();
        SimdF mbit1 = (((hash & b1) == b1)).as_float();
        SimdF mbit2 = (((hash & b2) == b2)).as_float();
        SimdF mbit3 = (((hash & b3) == b3)).as_float();
        SimdF maskBC = mbit2 ^ mbit3;

        SimdF sdx_m0 = dx ^ (mbit0 & sign);
        SimdF sdy_m1 = dy ^ (mbit1 & sign);
        SimdF sdz_m1 = dz ^ (mbit1 & sign);
        SimdF sdy_m0 = dy ^ (mbit0 & sign);

        SimdF ga = sdx_m0 + sdy_m1;
        SimdF gb = sdx_m0 + sdz_m1;
        SimdF gc = sdy_m0 + sdz_m1;

        SimdF groupBC = SimdF::blendv(gc, gb, mbit2);
        return SimdF::blendv(ga, groupBC, maskBC);
    }

    // Non-seamless (default) noise:
    //  'step' can be any positive value.
    // Seamless (tiling) noise:
    //  'step' must satisfy (1.0f / step) being a whole number (e.g. 0.5, 0.25, 0.125).
    //  The period is derived as (float)(1.0f / step) and passed directly to wrap().
    template<
        auto Scalar2DFunc,
        auto Simd2DFunc,
        auto Scalar3DFunc,
        auto Simd3DFunc,
        bool IsSeamless = false,
        bool IsDataAligned = false,
        bool HasTail = true
    >
    class BaseNoiseGenerator
    {
    public:
        static void gen2D(
            float* out,
            int seed,
            const glm::ivec2& resolution,
            const float step,
            const glm::vec2& initialOffset
        )
        {
            float gridResolution = 1.0f / step;
            if constexpr (IsSeamless)
            {
                gridResolution = floorf(gridResolution);
            }
            const glm::vec2 baseScale = glm::vec2(gridResolution) / glm::vec2(resolution.x);
            const int period = (int)gridResolution;

            SimdF xOffsets;
            if constexpr (SimdF::lanes == 8)
            {
                xOffsets = SimdF::set(7, 6, 5, 4, 3, 2, 1, 0);
            }
            else if constexpr (SimdF::lanes == 4)
            {
                xOffsets = SimdF::set(3, 2, 1, 0);
            }

            const SimdF xScale = SimdF::fill_lanes_with_value(baseScale.x);
            const SimdI vPeriod = SimdI::fill_lanes_with_value(period);
            const SimdI vSeed = SimdI::fill_lanes_with_value(seed);

            constexpr int LANE_COUNT = SimdF::lanes;
            const SimdF xStep = SimdF::fill_lanes_with_value((float)LANE_COUNT * baseScale.x);

            xOffsets += SimdF::fill_lanes_with_value(initialOffset.x);
            xOffsets *= xScale;

            for (int y = 0; y < resolution.y; y++)
            {
                const SimdF ySamplePoints = SimdF::fill_lanes_with_value((y + initialOffset.y) * baseScale.y);
                float* outRow = out + y * resolution.x;

                // SIMD loop
                int x = 0;
                SimdF xSamplePoints = xOffsets;
                for (; x + LANE_COUNT <= resolution.x; x += LANE_COUNT)
                {
                    SimdF values;
                    if constexpr (IsSeamless)
                    {
                        values = Simd2DFunc(xSamplePoints, ySamplePoints, vPeriod, vSeed);
                    }
                    else
                    {
                        values = Simd2DFunc(xSamplePoints, ySamplePoints, vSeed);
                    }

                    if constexpr (IsDataAligned)
                    {
                        values.store(outRow + x);
                    }
                    else
                    {
                        values.storeu(outRow + x);
                    }

                    xSamplePoints += xStep;
                }

                if constexpr (HasTail)
                {
                    // Scalar tail (handles resolution.x not divisible by LANE_COUNT)
                    for (; x < resolution.x; x++)
                    {
                        glm::vec2 p = (glm::vec2(x, y) + initialOffset) * baseScale;

                        float value;
                        if constexpr (IsSeamless)
                        {
                            value = Scalar2DFunc(p, period, seed);
                        }
                        else
                        {
                            value = Scalar2DFunc(p, seed);
                        }

                        outRow[x] = value;
                    }
                }
            }
        }

        static void genLayered2D(
            float* out,
            int seed,
            const glm::ivec2& resolution,
            const float step,
            const glm::vec2& initialOffset,
            int octaveCount,
            float lacunarity
        )
        {
            // Initialize whole array to zero
            const int resolutionArea = resolution.x * resolution.y;
            for (int i = 0; i < resolutionArea; i++)
            {
                out[i] = 0.0f;
            }

            // Calculate grid resolution
            float gridResolution = 1.0f / step;
            if constexpr (IsSeamless)
            {
                gridResolution = floorf(gridResolution);
            }
            const glm::vec2 invResolution = 1.0f / glm::vec2(resolution);
            const glm::vec2 baseScale = invResolution * gridResolution;

            // Set SIMD constants
            SimdF xOffsets;
            if constexpr (SimdF::lanes == 8)
            {
                xOffsets = SimdF::set(7, 6, 5, 4, 3, 2, 1, 0);
            }
            else if constexpr (SimdF::lanes == 4)
            {
                xOffsets = SimdF::set(3, 2, 1, 0);
            }

            constexpr int LANE_COUNT = SimdF::lanes;

            // Main loop
            float attenuation = 1.0f;
            for (int octave = 0; octave < octaveCount; octave++)
            {
                const int octaveSeed = seed + octave;
                const float invAttenuation = 1.0f / attenuation;

                const int period = (int)(gridResolution * attenuation);

                const glm::vec2 scale = baseScale * attenuation;

                const SimdF xScale = SimdF::fill_lanes_with_value(scale.x);
                const SimdI vSeed = SimdI::fill_lanes_with_value(octaveSeed);
                const SimdI vPeriod = SimdI::fill_lanes_with_value(period);
                const SimdF vInvAttenuation = SimdF::fill_lanes_with_value(invAttenuation);
                const SimdF xStep = SimdF::fill_lanes_with_value((float)LANE_COUNT * scale.x);

                const SimdF transformedXOffsets = (xOffsets + SimdF::fill_lanes_with_value(initialOffset.x)) * xScale;

                for (int y = 0; y < resolution.y; y++)
                {
                    const SimdF ySamplePoints = SimdF::fill_lanes_with_value((y + initialOffset.y) * scale.y);
                    float* outRow = out + y * resolution.x;

                    // SIMD loop
                    int x = 0;
                    SimdF xSamplePoints = transformedXOffsets;
                    for (; x + LANE_COUNT <= resolution.x; x += LANE_COUNT)
                    {
                        SimdF values;
                        if constexpr (IsSeamless)
                        {
                            values = Simd2DFunc(xSamplePoints, ySamplePoints, vPeriod, vSeed);
                        }
                        else
                        {
                            values = Simd2DFunc(xSamplePoints, ySamplePoints, vSeed);
                        }

                        SimdF oldValues;
                        if constexpr (IsDataAligned)
                        {
                            oldValues = SimdF::load(outRow + x);
                        }
                        else
                        {
                            oldValues = SimdF::loadu(outRow + x);
                        }

                        values = SimdF::mul_add(values, vInvAttenuation, oldValues);

                        if constexpr (IsDataAligned)
                        {
                            values.store(outRow + x);
                        }
                        else
                        {
                            values.storeu(outRow + x);
                        }

                        xSamplePoints += xStep;
                    }

                    if constexpr (HasTail)
                    {
                        // Scalar tail (handles resolution.x not divisible by LANE_COUNT)
                        for (; x < resolution.x; x++)
                        {
                            glm::vec2 p = (glm::vec2(x, y) + initialOffset) * scale;

                            float value;
                            if constexpr (IsSeamless)
                            {
                                value = Scalar2DFunc(p, period, octaveSeed);
                            }
                            else
                            {
                                value = Scalar2DFunc(p, octaveSeed);
                            }

                            outRow[x] += value * invAttenuation;
                        }
                    }
                }

                // Update by lacunarity
                attenuation *= lacunarity;
            }

            // Normalize array
            const float invMaxValue = 1.0f / sumOfGeometricSeries(1.0f, 1.0f / lacunarity, octaveCount);
            const SimdF vFactor = SimdF::fill_lanes_with_value(invMaxValue);

            int i = 0;
            for (; i + LANE_COUNT <= resolutionArea; i += LANE_COUNT)
            {
                if constexpr (IsDataAligned)
                {
                    SimdF values = SimdF::load(out + i);
                    values = SimdF::mul(values, vFactor);
                    values.store(out + i);
                }
                else
                {
                    SimdF values = SimdF::loadu(out + i);
                    values = SimdF::mul(values, vFactor);
                    values.storeu(out + i);
                }
            }

            if constexpr (HasTail)
            {
                for (; i < resolutionArea; i++)
                {
                    out[i] *= invMaxValue;
                }
            }
        }

        static void gen3D(
            float* out,
            int seed,
            const glm::ivec3& resolution,
            const float step,
            const glm::vec3& initialOffset
        )
        {
            float gridResolution = 1.0f / step;
            if constexpr (IsSeamless)
            {
                gridResolution = floorf(gridResolution);
            }
            const glm::vec3 baseScale = glm::vec3(gridResolution) / glm::vec3(resolution);
            const int period = (int)gridResolution;

            SimdF xOffsets;
            if constexpr (SimdF::lanes == 8)
            {
                xOffsets = SimdF::set(7, 6, 5, 4, 3, 2, 1, 0);
            }
            else if constexpr (SimdF::lanes == 4)
            {
                xOffsets = SimdF::set(3, 2, 1, 0);
            }

            const SimdF xScale = SimdF::fill_lanes_with_value(baseScale.x);
            const SimdI vPeriod = SimdI::fill_lanes_with_value(period);
            const SimdI vSeed = SimdI::fill_lanes_with_value(seed);

            constexpr int LANE_COUNT = SimdF::lanes;
            const SimdF xStep = SimdF::fill_lanes_with_value((float)LANE_COUNT * baseScale.x);

            xOffsets += SimdF::fill_lanes_with_value(initialOffset.x);
            xOffsets *= xScale;

            for (int z = 0; z < resolution.z; z++)
            {
                const SimdF zSamplePoints = SimdF::fill_lanes_with_value((z + initialOffset.z) * baseScale.z);
                for (int y = 0; y < resolution.y; y++)
                {
                    const SimdF ySamplePoints = SimdF::fill_lanes_with_value((y + initialOffset.y) * baseScale.y);
                    float* outRow = out + z * resolution.y * resolution.x + y * resolution.x;

                    // SIMD loop
                    int x = 0;
                    SimdF xSamplePoints = xOffsets;
                    for (; x + LANE_COUNT <= resolution.x; x += LANE_COUNT)
                    {
                        SimdF values;
                        if constexpr (IsSeamless)
                        {
                            values = Simd3DFunc(xSamplePoints, ySamplePoints, zSamplePoints, vPeriod, vSeed);
                        }
                        else
                        {
                            values = Simd3DFunc(xSamplePoints, ySamplePoints, zSamplePoints, vSeed);
                        }

                        if constexpr (IsDataAligned)
                        {
                            values.store(outRow + x);
                        }
                        else
                        {
                            values.storeu(outRow + x);
                        }

                        xSamplePoints += xStep;
                    }

                    if constexpr (HasTail)
                    {
                        // Scalar tail
                        for (; x < resolution.x; x++)
                        {
                            glm::vec3 p = (glm::vec3(x, y, z) + initialOffset) * baseScale;

                            float value;
                            if constexpr (IsSeamless)
                            {
                                value = Scalar3DFunc(p, period, seed);
                            }
                            else
                            {
                                value = Scalar3DFunc(p, seed);
                            }

                            outRow[x] = value;
                        }
                    }
                }
            }
        }

        static void genLayered3D(
            float* out,
            int seed,
            const glm::ivec3& resolution,
            const float step,
            const glm::vec3& initialOffset,
            int octaveCount,
            float lacunarity
        )
        {
            // Initialize whole array to zero
            const int resolutionVolume = resolution.x * resolution.y * resolution.z;
            for (int i = 0; i < resolutionVolume; i++)
            {
                out[i] = 0.0f;
            }

            // Calculate grid resolution
            float gridResolution = 1.0f / step;
            if constexpr (IsSeamless)
            {
                gridResolution = floorf(gridResolution);
            }
            const glm::vec3 invResolution = 1.0f / glm::vec3(resolution);
            const glm::vec3 baseScale = invResolution * gridResolution;

            // Set SIMD constants
            SimdF xOffsets;
            if constexpr (SimdF::lanes == 8)
            {
                xOffsets = SimdF::set(7, 6, 5, 4, 3, 2, 1, 0);
            }
            else if constexpr (SimdF::lanes == 4)
            {
                xOffsets = SimdF::set(3, 2, 1, 0);
            }

            constexpr int LANE_COUNT = SimdF::lanes;

            // Main loop
            float attenuation = 1.0f;
            for (int octave = 0; octave < octaveCount; octave++)
            {
                const int octaveSeed = seed + octave;
                const float invAttenuation = 1.0f / attenuation;

                const glm::vec3 scale = baseScale * attenuation;

                const int period = (int)(gridResolution * attenuation);

                const SimdF xScale = SimdF::fill_lanes_with_value(scale.x);
                const SimdI vSeed = SimdI::fill_lanes_with_value(octaveSeed);
                const SimdI vPeriod = SimdI::fill_lanes_with_value(period);
                const SimdF vInvAttenuation = SimdF::fill_lanes_with_value(invAttenuation);
                const SimdF xStep = SimdF::fill_lanes_with_value((float)LANE_COUNT * scale.x);

                const SimdF transformedXOffsets = (xOffsets + SimdF::fill_lanes_with_value(initialOffset.x)) * xScale;

                for (int z = 0; z < resolution.z; z++)
                {
                    const SimdF zSamplePoints = SimdF::fill_lanes_with_value((z + initialOffset.z) * scale.z);
                    for (int y = 0; y < resolution.y; y++)
                    {
                        const SimdF ySamplePoints = SimdF::fill_lanes_with_value((y + initialOffset.y) * scale.y);
                        float* outRow = out + z * resolution.y * resolution.x + y * resolution.x;

                        // SIMD loop
                        int x = 0;
                        SimdF xSamplePoints = transformedXOffsets;
                        for (; x + LANE_COUNT <= resolution.x; x += LANE_COUNT)
                        {
                            SimdF values;
                            if constexpr (IsSeamless)
                            {
                                values = Simd3DFunc(xSamplePoints, ySamplePoints, zSamplePoints, vPeriod, vSeed);
                            }
                            else
                            {
                                values = Simd3DFunc(xSamplePoints, ySamplePoints, zSamplePoints, vSeed);
                            }

                            SimdF oldValues;
                            if constexpr (IsDataAligned)
                            {
                                oldValues = SimdF::load(outRow + x);
                            }
                            else
                            {
                                oldValues = SimdF::loadu(outRow + x);
                            }

                            values = SimdF::mul_add(values, vInvAttenuation, oldValues);

                            if constexpr (IsDataAligned)
                            {
                                values.store(outRow + x);
                            }
                            else
                            {
                                values.storeu(outRow + x);
                            }

                            xSamplePoints += xStep;
                        }

                        if constexpr (HasTail)
                        {
                            // Scalar tail
                            for (; x < resolution.x; x++)
                            {
                                glm::vec3 p = (glm::vec3(x, y, z) + initialOffset) * scale;

                                float value;
                                if constexpr (IsSeamless)
                                {
                                    value = Scalar3DFunc(p, period, octaveSeed);
                                }
                                else
                                {
                                    value = Scalar3DFunc(p, octaveSeed);
                                }

                                outRow[x] += value * invAttenuation;
                            }
                        }
                    }
                }

                // Update by lacunarity
                attenuation *= lacunarity;
            }

            // Normalize array
            const float invMaxValue = 1.0f / sumOfGeometricSeries(1.0f, 1.0f / lacunarity, octaveCount);
            const SimdF vFactor = SimdF::fill_lanes_with_value(invMaxValue);

            int i = 0;
            for (; i + LANE_COUNT <= resolutionVolume; i += LANE_COUNT)
            {
                if constexpr (IsDataAligned)
                {
                    SimdF values = SimdF::load(out + i);
                    values = SimdF::mul(values, vFactor);
                    values.store(out + i);
                }
                else
                {
                    SimdF values = SimdF::loadu(out + i);
                    values = SimdF::mul(values, vFactor);
                    values.storeu(out + i);
                }
            }

            if constexpr (HasTail)
            {
                for (; i < resolutionVolume; i++)
                {
                    out[i] *= invMaxValue;
                }
            }
        }
    };
}

