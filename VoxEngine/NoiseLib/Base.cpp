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

	SimdI hash_int3_simd(const SimdI& vx, const SimdI& vy, const SimdI& vz)
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


    float grad2(uint32_t hash, const glm::vec2& p)
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

    float grad3(uint32_t hash, const glm::vec3& p)
    {
        constexpr glm::vec3 gradients[16] = {
            { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
            { 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
            { 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1},
            { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
        };
        return glm::dot(gradients[hash & 15u], p);
    }

    SimdF grad2_simd(const SimdI& hash, const SimdF& dx, const SimdF& dy)
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

    SimdF grad3_simd(const SimdI& hash, const SimdF& dx, const SimdF& dy, const SimdF& dz)
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
}