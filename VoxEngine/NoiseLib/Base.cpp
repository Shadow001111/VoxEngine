#include "Base.h"

namespace NoiseLib::Base
{
    float sumOfGeometricSeries(float firstTerm, float commonRatio, int numberOfTerms)
    {
        if (commonRatio == 1.0f) return firstTerm * numberOfTerms;
        return firstTerm * (1.0f - powf(commonRatio, numberOfTerms)) / (1.0f - commonRatio);
    }


    uint32_t hash_int2(const glm::ivec2& v)
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

    uint32_t hash_int3(const glm::ivec3& v)
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

	SimdI hash_int2_simd(const SimdI& vx, const SimdI& vy)
	{
        const SimdI MAGIC = SimdI::fill_lanes_with_value((int)0x9e3779b9u);

        SimdI h = SimdI::fill_lanes_with_value((int)0xdeadbeefu);

        // Mix one component into h
        auto mix = [&](SimdI val)
            {
                SimdI t = val + MAGIC;
                t = t + (h << 6);
                t = t + (h >> 2);
                h = h ^ t;
            };

        mix(vx);
        mix(vy);

        // Murmur-style avalanche
        h = h ^ (h >> 16);
        h = h * SimdI::fill_lanes_with_value((int)0x85ebca6bu);
        h = h ^ (h >> 13);
        h = h * SimdI::fill_lanes_with_value((int)0xc2b2ae35u);
        h = h ^ (h >> 16);
        return h;
	}

	SimdI hash_int3_simd(const SimdI& vx, const SimdI& vy, const SimdI& vz)
	{
        const SimdI MAGIC = SimdI::fill_lanes_with_value((int)0x9e3779b9u);

        SimdI h = SimdI::fill_lanes_with_value((int)0xdeadbeefu);

        // Mix one component into h
        auto mix = [&](SimdI val)
            {
                SimdI t = val + MAGIC;
                t = t + (h << 6);
                t = t + (h >> 2);
                h = h ^ t;
            };

        mix(vx);
        mix(vy);
        mix(vz);

        // Murmur-style avalanche
        h = h ^ (h >> 16);
        h = h * SimdI::fill_lanes_with_value((int)0x85ebca6bu);
        h = h ^ (h >> 13);
        h = h * SimdI::fill_lanes_with_value((int)0xc2b2ae35u);
        h = h ^ (h >> 16);
        return h;
	}


    int wrap(int idx, int period)
    {
        return idx - period * floorf((float)idx / (float)period);
    }

    glm::ivec2 wrap(const glm::ivec2& idx, int period)
    {
        return glm::vec2(idx) - (float)period * glm::floor(glm::vec2(idx) / (float)period);
    }

    glm::ivec3 wrap(const glm::ivec3& idx, int period)
    {
        return glm::vec3(idx) - (float)period * glm::floor(glm::vec3(idx) / (float)period);
    }

    SimdI wrap_simd(const SimdI& idx, const SimdI& period)
    {
        auto idxF = idx.to_float();
        auto periodF = period.to_float();
        auto div = idxF / periodF;
        //return (idxF - periodF * SimdF::floor(div)).to_int32();
        return SimdF::neg_mul_add(periodF, SimdF::floor(div), idxF).to_int32();
    }
}