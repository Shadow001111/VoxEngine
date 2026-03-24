#include "Simplex.h"

namespace NoiseLib::Simplex
{
    // =========================================================================
    // Scalar helpers
    // =========================================================================

    static float grad2(uint32_t hash, const glm::vec2& p)
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

    static float grad3(uint32_t hash, const glm::vec3& p)
    {
        static const glm::vec3 gradients[16] = {
            { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
            { 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
            { 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1},
            { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
        };
        return glm::dot(gradients[hash & 15u], p);
    }

    float scalar2D(const glm::vec2& p, int seed)
    {
        constexpr float F2 = 0.36602540378f;
        constexpr float G2 = 0.21132486541f;

        const float s = (p.x + p.y) * F2;
        const glm::vec2 ps = glm::floor(p + s);
        const glm::ivec2 pi = glm::ivec2(ps);
        const float t = (ps.x + ps.y) * G2;
        const glm::vec2 d0 = p - ps + t;

        const glm::ivec2 i1 = (d0.x > d0.y) ? glm::ivec2(1, 0) : glm::ivec2(0, 1);
        const glm::vec2 d1 = d0 - glm::vec2(i1) + G2;
        const glm::vec2 d2 = d0 - 1.0f + 2.0f * G2;

        float n = 0.0f;
        auto contrib = [&](const glm::ivec2& cell, const glm::vec2& d) -> float
            {
                float tc = 0.5f - glm::dot(d, d);
                if (tc <= 0.0f) return 0.0f;
                tc *= tc;
                return tc * tc * grad2(NoiseLib::Base::hash_int2(cell) + seed, d);
            };

        n += contrib(pi, d0);
        n += contrib(pi + i1, d1);
        n += contrib(pi + glm::ivec2(1, 1), d2);
        return n * 70.0f;
    }

    float scalar3D(const glm::vec3& p, int seed)
    {
        constexpr float F3 = 1.0f / 3.0f;
        constexpr float G3 = 1.0f / 6.0f;

        const float s = (p.x + p.y + p.z) * F3;
        const glm::vec3 ps = glm::floor(p + s);
        const glm::ivec3 pi = glm::ivec3(ps);
        const float t = (ps.x + ps.y + ps.z) * G3;
        const glm::vec3 d0 = p - ps + t;

        // TODO: Use LUT
        glm::ivec3 i1, i2;
        if (d0.x >= d0.y)
        {
            if (d0.y >= d0.z) { i1 = { 1,0,0 }; i2 = { 1,1,0 }; }
            else if (d0.x >= d0.z) { i1 = { 1,0,0 }; i2 = { 1,0,1 }; }
            else { i1 = { 0,0,1 }; i2 = { 1,0,1 }; }
        }
        else
        {
            if (d0.y < d0.z) { i1 = { 0,0,1 }; i2 = { 0,1,1 }; }
            else if (d0.x < d0.z) { i1 = { 0,1,0 }; i2 = { 0,1,1 }; }
            else { i1 = { 0,1,0 }; i2 = { 1,1,0 }; }
        }

        const glm::vec3 d1 = d0 - glm::vec3(i1) + G3;
        const glm::vec3 d2 = d0 - glm::vec3(i2) + 2.0f * G3;
        const glm::vec3 d3 = d0 - 1.0f + 3.0f * G3;

        float n = 0.0f;
        auto contrib3 = [&](const glm::ivec3& cell, const glm::vec3& d) -> float
            {
                float tc = 0.6f - glm::dot(d, d);
                if (tc <= 0.0f) return 0.0f;
                tc *= tc;
                return tc * tc * grad3(NoiseLib::Base::hash_int3(cell) + seed, d);
            };

        n += contrib3(pi, d0);
        n += contrib3(pi + i1, d1);
        n += contrib3(pi + i2, d2);
        n += contrib3(pi + glm::ivec3(1, 1, 1), d3);
        return n * 32.0f;
    }

    // =========================================================================
    // SIMD helpers
    // =========================================================================

    static SimdF grad2_simd(const SimdI& hash, const SimdF& dx, const SimdF& dy)
    {
        const SimdI one =  SimdI::fill_lanes_with_value(1);
        const SimdI two =  SimdI::fill_lanes_with_value(2);
        const SimdI four = SimdI::fill_lanes_with_value(4);
        const SimdF SIGN = SimdF::fill_lanes_with_value(-0.0f);

        SimdF mbit0 = (((hash & one ) == one )).as_float();
        SimdF mbit1 = (((hash & two ) == two )).as_float();
        SimdF mbit2 = (((hash & four) == four)).as_float();

        SimdF gx = SimdF::blendv(dx, dx ^ SIGN, mbit0);
        SimdF gy = SimdF::blendv(dy, dy ^ SIGN, mbit1);
        SimdF both = gx + gy;

        SimdF single_unsigned = SimdF::blendv(dx, dy, mbit1);
        SimdF single = SimdF::blendv(single_unsigned, single_unsigned ^ SIGN, mbit0);

        return SimdF::blendv(both, single, mbit2);
    }

    static SimdF grad3_simd(const SimdI& hash, const SimdF& dx, const SimdF& dy, const SimdF& dz)
    {
        const SimdI one = SimdI::fill_lanes_with_value(1);
        const SimdF SIGN = SimdF::fill_lanes_with_value(-0.0f);

        SimdI bit0 = hash & one;
        SimdI bit1 = (hash >> 1) & one;
        SimdI bit2 = (hash >> 2) & one;
        SimdI bit3 = (hash >> 3) & one;

        SimdF mbit0 = (bit0 == one).as_float();
        SimdF mbit1 = (bit1 == one).as_float();
        SimdF mbit2 = (bit2 == one).as_float();
        SimdF mask_bc = ((bit2 ^ bit3) == one).as_float();

        // Group A (bit3==bit2): ±dx ± dy
        // Group B (bit3!=bit2, bit2==1): ±dx ± dz
        // Group C (bit3!=bit2, bit2==0): ±dy ± dz
        SimdF ga = SimdF::blendv(dx, dx ^ SIGN, mbit0) + SimdF::blendv(dy, dy ^ SIGN, mbit1);
        SimdF gb = SimdF::blendv(dx, dx ^ SIGN, mbit0) + SimdF::blendv(dz, dz ^ SIGN, mbit1);
        SimdF gc = SimdF::blendv(dy, dy ^ SIGN, mbit0) + SimdF::blendv(dz, dz ^ SIGN, mbit1);

        SimdF groupBC = SimdF::blendv(gc, gb, mbit2);
        return SimdF::blendv(ga, groupBC, mask_bc);
    }

    SimdF simd2D(const SimdF& vpx, const SimdF& vpy, const SimdI& vseed)
    {
        constexpr float SCALAR_F2 = 0.36602540378f;   // (sqrt(3)-1)/2
        constexpr float SCALAR_G2 = 0.21132486541f;   // (3-sqrt(3))/6

        const SimdF F2 =    SimdF::fill_lanes_with_value(SCALAR_F2);
        const SimdF G2 =    SimdF::fill_lanes_with_value(SCALAR_G2);
        const SimdF G2x2 =  SimdF::fill_lanes_with_value(SCALAR_G2 * 2.0f);
        const SimdF oneF =  SimdF::fill_lanes_with_value(1.0f);
        const SimdI oneI =  SimdI::fill_lanes_with_value(1);
        const SimdF half =  SimdF::fill_lanes_with_value(0.5f);
        const SimdF zero =  SimdF::fill_lanes_with_zero();

        // --- Skew input point into the simplex lattice -----------------------
        SimdF s = (vpx + vpy) * F2;
        SimdF pfx = SimdF::floor(vpx + s);
        SimdF pfy = SimdF::floor(vpy + s);

        // Integer cell origin (truncates toward zero)
        SimdI pix = pfx.to_int32();
        SimdI piy = pfy.to_int32();

        // --- Unskew back and get offset from corner 0 ------------------------
        SimdF t   = (pfx + pfy) * G2;
        SimdF d0x = (vpx - pfx) + t;
        SimdF d0y = (vpy - pfy) + t;

        // --- Triangle selection (branchless) ---------------------------------
        // Scalar: i1 = (d0.x > d0.y) ? (1,0) : (0,1)
        // AVX2:   compare gives an all-ones/-zeros lane mask; AND with 1 gives 0 or 1
        SimdF vcmp = d0x > d0y;
        SimdI vcmpI = vcmp.as_int32();

        SimdI vi1x = vcmpI & oneI;
        SimdI vi1y = SimdI::bitwise_andnot(vcmpI, oneI);

        // --- Corner offsets --------------------------------------------------
        SimdF vd1x = (d0x - vi1x.to_float()) + G2;
        SimdF vd1y = (d0y - vi1y.to_float()) + G2;
        SimdF vd2x = (d0x - oneF)            + G2x2;
        SimdF vd2y = (d0y - oneF)            + G2x2;

        // --- Wrapped cell indices for the three corners ----------------------
        SimdI vc0x = pix;
        SimdI vc0y = piy;
        SimdI vc1x = pix + vi1x;
        SimdI vc1y = piy + vi1y;
        SimdI vc2x = pix + oneI;
        SimdI vc2y = piy + oneI;

        // --- Hashes ----------------------------------------------------------
        SimdI vh0 = NoiseLib::Base::hash_int2_simd(vc0x, vc0y) + vseed;
        SimdI vh1 = NoiseLib::Base::hash_int2_simd(vc1x, vc1y) + vseed;
        SimdI vh2 = NoiseLib::Base::hash_int2_simd(vc2x, vc2y) + vseed;

        // --- Radial kernel: max(0, 0.5 - |d|^2)^4 * grad --------------------
        // The t > 0 guard is a mask: inactive lanes are zeroed before squaring,
        // so they contribute exactly 0 to the sum — no branches, no blends on t4.
        auto kernel = [&](SimdI h, SimdF dx, SimdF dy) -> SimdF
        {
            SimdF t = half - (dx * dx + dy * dy);
            SimdF ok = t > zero;
            t = t & ok;     // zero out inactive lanes
            SimdF t2 = t * t;
            SimdF t4 = t2 * t2;
            return (t4 * grad2_simd(h, dx, dy)) & ok;
        };

        SimdF vn = kernel(vh0, d0x, d0y) + kernel(vh1, vd1x, vd1y) + kernel(vh2, vd2x, vd2y);

        const SimdF outputFactor = SimdF::fill_lanes_with_value(70.0f);

        return vn * outputFactor;
    }

    SimdF simd3D(const SimdF& vpx, const SimdF& vpy, const SimdF& vpz, const SimdI& vseed)
    {
        constexpr float SCALAR_F3 = 1.0f / 3.0f;
        constexpr float SCALAR_G3 = 1.0f / 6.0f;

        const SimdF F3 = SimdF::fill_lanes_with_value(SCALAR_F3);
        const SimdF G3 = SimdF::fill_lanes_with_value(SCALAR_G3);
        const SimdF G3x2 = SimdF::fill_lanes_with_value(SCALAR_G3 * 2.0f);
        const SimdF G3x3 = SimdF::fill_lanes_with_value(SCALAR_G3 * 3.0f);
        const SimdF oneF = SimdF::fill_lanes_with_value(1.0f);
        const SimdI oneI = SimdI::fill_lanes_with_value(1);
        const SimdI allOnes = SimdI::fill_lanes_with_value(-1);
        const SimdF thresh = SimdF::fill_lanes_with_value(0.6f);
        const SimdF zero = SimdF::fill_lanes_with_zero();

        // --- Skew into simplex lattice -----------------------------------------
        SimdF s = (vpx + vpy + vpz) * F3;
        SimdF pfx = SimdF::floor(vpx + s);
        SimdF pfy = SimdF::floor(vpy + s);
        SimdF pfz = SimdF::floor(vpz + s);

        SimdI pix = pfx.to_int32();
        SimdI piy = pfy.to_int32();
        SimdI piz = pfz.to_int32();

        // --- Unskew to get offset from corner 0 --------------------------------
        SimdF t = (pfx + pfy + pfz) * G3;
        SimdF d0x = (vpx - pfx) + t;
        SimdF d0y = (vpy - pfy) + t;
        SimdF d0z = (vpz - pfz) + t;

        // --- Branchless simplex ordering ----------------------------------------
        // Integer masks: -1 (all-ones) = true, 0 = false
        SimdI mxy = (d0x >= d0y).as_int32();   // x >= y
        SimdI mxz = (d0x >= d0z).as_int32();   // x >= z
        SimdI myz = (d0y >= d0z).as_int32();   // y >= z

        SimdI notMxy = mxy ^ allOnes;
        SimdI notMxz = mxz ^ allOnes;
        SimdI notMyz = myz ^ allOnes;

        // i1: unit vector for the largest component
        //   i1x = (x>=y) AND (x>=z)
        //   i1y = NOT(x>=y) AND (y>=z) → bitwise_andnot(a,b) = (~a)&b
        //   i1z = remainder (exactly one of i1x/y/z is 1)
        SimdI vi1x = (mxy & mxz) & oneI;
        SimdI vi1y = SimdI::bitwise_andnot(mxy, myz) & oneI;
        SimdI vi1z = oneI - vi1x - vi1y;

        // i2: "not the minimum" gets 1
        //   x is min iff ~mxy AND ~mxz  →  i2x = mxy | mxz
        //   y is min iff mxy AND ~myz   →  i2y = ~mxy | myz
        //   z is min iff mxz AND myz    →  i2z = ~mxz | ~myz
        SimdI vi2x = (mxy | mxz) & oneI;
        SimdI vi2y = (notMxy | myz) & oneI;
        SimdI vi2z = (notMxz | notMyz) & oneI;

        // --- Corner distance vectors -------------------------------------------
        SimdF fi1x = vi1x.to_float(), fi1y = vi1y.to_float(), fi1z = vi1z.to_float();
        SimdF fi2x = vi2x.to_float(), fi2y = vi2y.to_float(), fi2z = vi2z.to_float();

        SimdF d1x = d0x - fi1x + G3, d1y = d0y - fi1y + G3, d1z = d0z - fi1z + G3;
        SimdF d2x = d0x - fi2x + G3x2, d2y = d0y - fi2y + G3x2, d2z = d0z - fi2z + G3x2;
        SimdF d3x = d0x - oneF + G3x3, d3y = d0y - oneF + G3x3, d3z = d0z - oneF + G3x3;

        // --- Wrapped cell indices for hashing ----------------------------------
        SimdI vc0x = pix;
        SimdI vc0y = piy;
        SimdI vc0z = piz;

        SimdI vc1x = pix + vi1x;
        SimdI vc1y = piy + vi1y;
        SimdI vc1z = piz + vi1z;

        SimdI vc2x = pix + vi2x;
        SimdI vc2y = piy + vi2y;
        SimdI vc2z = piz + vi2z;

        SimdI vc3x = pix + oneI;
        SimdI vc3y = piy + oneI;
        SimdI vc3z = piz + oneI;

        // --- Hashes ------------------------------------------------------------
        SimdI vh0 = NoiseLib::Base::hash_int3_simd(vc0x, vc0y, vc0z) + vseed;
        SimdI vh1 = NoiseLib::Base::hash_int3_simd(vc1x, vc1y, vc1z) + vseed;
        SimdI vh2 = NoiseLib::Base::hash_int3_simd(vc2x, vc2y, vc2z) + vseed;
        SimdI vh3 = NoiseLib::Base::hash_int3_simd(vc3x, vc3y, vc3z) + vseed;

        // --- Radial kernel: max(0, 0.6 - |d|^2)^4 * grad ----------------------
        auto kernel = [&](SimdI h, SimdF dx, SimdF dy, SimdF dz) -> SimdF
            {
                SimdF tc = thresh - (dx * dx + dy * dy + dz * dz);
                SimdF ok = tc > zero;
                tc = tc & ok;            // zero inactive lanes before squaring
                SimdF tc2 = tc * tc;
                SimdF tc4 = tc2 * tc2;
                return (tc4 * grad3_simd(h, dx, dy, dz)) & ok;
            };

        SimdF vn = kernel(vh0, d0x, d0y, d0z)
            + kernel(vh1, d1x, d1y, d1z)
            + kernel(vh2, d2x, d2y, d2z)
            + kernel(vh3, d3x, d3y, d3z);

        return vn * SimdF::fill_lanes_with_value(32.0f);
    }
}