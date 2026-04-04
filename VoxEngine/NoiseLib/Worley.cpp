#include "Worley.h"

#include <algorithm>

namespace NoiseLib::Worley
{
    // =========================================================================
    // Scalar helpers
    // =========================================================================

    static __forceinline int32_t hash1(uint32_t x) noexcept
    {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= (x >> 15);
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    static __forceinline float rand01(uint32_t seed)
    {
        constexpr float invDivisor = 1.0f / float(0x01000000);
        return float(hash1(seed) & 0x00FFFFFF) * invDivisor;
    }

    float scalar2D(const glm::vec2& p, int seed)
    {
		const int cellX = static_cast<int>(std::floor(p.x));
        const int cellY = static_cast<int>(std::floor(p.y));
        float minDistSquared = 1e30f;
        for (int gx = cellX - 1; gx <= cellX + 1; gx++)
        for (int gy = cellY - 1; gy <= cellY + 1; gy++)
        {
            uint32_t h = hash1((gx * 374761393) ^ (gy * 668265263) ^ seed);
            float fx = (float)gx + rand01(h ^ 0xA341316C);
            float fy = (float)gy + rand01(h ^ 0xC8013EA4);
            float dx = fx - p.x;
            float dy = fy - p.y;
            float d2 = dx * dx + dy * dy;
            minDistSquared = std::min(minDistSquared, d2);
        }
        float minDist = std::sqrt(minDistSquared);
        minDist *= 0.7071067811865475f; // 1 / sqrt(2)
		return minDist;
    }

    float scalar3D(const glm::vec3& p, int seed)
    {
		const int cellX = static_cast<int>(std::floor(p.x));
        const int cellY = static_cast<int>(std::floor(p.y));
        const int cellZ = static_cast<int>(std::floor(p.z));
        float minDistSquared = 1e30f;
        for (int gx = cellX - 1; gx <= cellX + 1; gx++)
        for (int gy = cellY - 1; gy <= cellY + 1; gy++)
        for (int gz = cellZ - 1; gz <= cellZ + 1; gz++)
        {
            uint32_t h = hash1(gx * 374761393 ^ gy * 668265263 ^ gz * 2147483647 ^ seed);
            float fx = gx + rand01(h ^ 0xA341316C);
            float fy = gy + rand01(h ^ 0xC8013EA4);
            float fz = gz + rand01(h ^ 0xAD90777D);
            float dx = fx - p.x;
            float dy = fy - p.y;
            float dz = fz - p.z;
            float d2 = dx * dx + dy * dy + dz * dz;
            minDistSquared = std::min(minDistSquared, d2);
        }
        float minDist = std::sqrt(minDistSquared);
		minDist *= 0.5773502691896258f; // 1 / sqrt(3)
		return minDist;
    }

    // =========================================================================
    // SIMD helpers
    // =========================================================================

    static __forceinline SimdI hash1_simd(SimdI x) noexcept
    {
        x ^= (x >> 16);
        x *= SimdI(0x7feb352d);
        x ^= (x >> 15);
        x *= SimdI(0x846ca68b);
        x ^= (x >> 16);
        return x;
    }

    static __forceinline SimdF rand01_simd(const SimdI& seed)
    {
		constexpr float invDivisor = 1.0f / float(0x01000000);
        return (hash1_simd(seed) & 0x00FFFFFFu).to_float() * SimdF(invDivisor);
    }

    static __forceinline void feature_point_simd(
        const SimdI& gx, const SimdI& gy,
        const SimdI& seed,
        SimdF& fx, SimdF& fy
    ) noexcept
    {
        SimdI h = hash1_simd(
            (gx * SimdI(374761393)) ^
            (gy * SimdI(668265263)) ^
            seed);

        fx = gx.to_float() + rand01_simd(h ^ SimdI(0xA341316C));
        fy = gy.to_float() + rand01_simd(h ^ SimdI(0xC8013EA4));
    }

    static __forceinline void feature_point_simd(
        const SimdI& gx, const SimdI& gy, const SimdI& gz,
        const SimdI& seed,
        SimdF& fx, SimdF& fy, SimdF& fz
    ) noexcept
    {
        SimdI h = hash1_simd(
            gx * SimdI(374761393) ^
            gy * SimdI(668265263) ^
            gz * SimdI(2147483647) ^
            seed);

        fx = gx.to_float() + rand01_simd(h ^ SimdI(0xA341316C));
        fy = gy.to_float() + rand01_simd(h ^ SimdI(0xC8013EA4));
        fz = gz.to_float() + rand01_simd(h ^ SimdI(0xAD90777D));
    }

    SimdF simd2D(const SimdF& vpx, const SimdF& vpy, const SimdI& vseed)
    {
        const SimdI cellX = SimdF::floor(vpx).to_int32();
        const SimdI cellY = SimdF::floor(vpy).to_int32();

        const SimdI x0 = cellX + SimdI(-1);
        const SimdI x1 = cellX;
        const SimdI x2 = cellX + SimdI(1);

        const SimdI y0 = cellY + SimdI(-1);
        const SimdI y1 = cellY;
        const SimdI y2 = cellY + SimdI(1);

        SimdF minDistSquared(1e30f);

        auto eval = [&](const SimdI& gx, const SimdI& gy) noexcept
            {
                SimdF px, py;
                feature_point_simd(gx, gy, vseed, px, py);

                const SimdF dx = px - vpx;
                const SimdF dy = py - vpy;

                const SimdF d2 = SimdF::mul_add(dy, dy, dx * dx);
                minDistSquared = SimdF::min(minDistSquared, d2);
            };

        eval(x0, y0); eval(x1, y0); eval(x2, y0);
        eval(x0, y1); eval(x1, y1); eval(x2, y1);
        eval(x0, y2); eval(x1, y2); eval(x2, y2);

        SimdF minDist = SimdF::sqrt(minDistSquared);
        minDist *= SimdF(0.7071067811865475f); // 1 / sqrt(2)
        return minDist;
    }

    SimdF simd3D(const SimdF& vpx, const SimdF& vpy, const SimdF& vpz, const SimdI& vseed)
    {
        const SimdI cellX = SimdF::floor(vpx).to_int32();
        const SimdI cellY = SimdF::floor(vpy).to_int32();
        const SimdI cellZ = SimdF::floor(vpz).to_int32();

        const SimdI x0 = cellX + SimdI(-1);
        const SimdI x1 = cellX;
        const SimdI x2 = cellX + SimdI(1);

        const SimdI y0 = cellY + SimdI(-1);
        const SimdI y1 = cellY;
        const SimdI y2 = cellY + SimdI(1);

        const SimdI z0 = cellZ + SimdI(-1);
        const SimdI z1 = cellZ;
        const SimdI z2 = cellZ + SimdI(1);

        SimdF minDistSquared(1e30f);

        auto eval = [&](const SimdI& gx, const SimdI& gy, const SimdI& gz) noexcept
            {
                SimdF px, py, pz;
                feature_point_simd(gx, gy, gz, vseed, px, py, pz);

                const SimdF dx = px - vpx;
                const SimdF dy = py - vpy;
                const SimdF dz = pz - vpz;

                const SimdF d2 = SimdF::mul_add(dz, dz, SimdF::mul_add(dy, dy, dx * dx));
                minDistSquared = SimdF::min(minDistSquared, d2);
            };

        eval(x0, y0, z0); eval(x1, y0, z0); eval(x2, y0, z0);
        eval(x0, y1, z0); eval(x1, y1, z0); eval(x2, y1, z0);
        eval(x0, y2, z0); eval(x1, y2, z0); eval(x2, y2, z0);

        eval(x0, y0, z1); eval(x1, y0, z1); eval(x2, y0, z1);
        eval(x0, y1, z1); eval(x1, y1, z1); eval(x2, y1, z1);
        eval(x0, y2, z1); eval(x1, y2, z1); eval(x2, y2, z1);

        eval(x0, y0, z2); eval(x1, y0, z2); eval(x2, y0, z2);
        eval(x0, y1, z2); eval(x1, y1, z2); eval(x2, y1, z2);
        eval(x0, y2, z2); eval(x1, y2, z2); eval(x2, y2, z2);

        SimdF minDist = SimdF::sqrt(minDistSquared);
        minDist *= SimdF(0.5773502691896258f); // 1 / sqrt(3)
        return minDist;
    }
}