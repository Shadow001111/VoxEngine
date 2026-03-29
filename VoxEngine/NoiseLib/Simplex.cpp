#include "Simplex.h"

namespace NoiseLib::Simplex
{
    // =========================================================================
    // Scalar helpers
    // =========================================================================

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
                return tc * tc * NoiseLib::Base::grad2(NoiseLib::Base::hash_int2(cell) + seed, d);
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
                return tc * tc * NoiseLib::Base::grad3(NoiseLib::Base::hash_int3(cell) + seed, d);
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

    SimdF simd2D(const SimdF& vpx, const SimdF& vpy, const SimdI& vseed)
    {
        constexpr float SCALAR_F2 = 0.36602540378f;   // (sqrt(3)-1)/2
        constexpr float SCALAR_G2 = 0.21132486541f;   // (3-sqrt(3))/6

        const SimdF F2 =    SimdF(SCALAR_F2);
        const SimdF G2 =    SimdF(SCALAR_G2);
        const SimdF G2x2 =  SimdF(SCALAR_G2 * 2.0f);
        const SimdF oneF =  SimdF(1.0f);
        const SimdI oneI =  SimdI(1);
        const SimdF thresh =SimdF(0.5f);
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
        SimdF vcmp = d0x > d0y;
        SimdI vcmpI = vcmp.as_int32();

        SimdI vi1x = vcmpI & oneI;
        SimdI vi1y = SimdI::bitwise_andnot(vcmpI, oneI);

        // --- Corner offsets --------------------------------------------------
        SimdF d1x = (d0x - vi1x.to_float()) + G2;
        SimdF d1y = (d0y - vi1y.to_float()) + G2;
        SimdF d2x = (d0x - oneF)            + G2x2;
        SimdF d2y = (d0y - oneF)            + G2x2;

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
        SimdF tc0 = SimdF::neg_mul_add(d0x, d0x, SimdF::neg_mul_add(d0y, d0y, thresh));
        SimdF tc1 = SimdF::neg_mul_add(d1x, d1x, SimdF::neg_mul_add(d1y, d1y, thresh));
        SimdF tc2 = SimdF::neg_mul_add(d2x, d2x, SimdF::neg_mul_add(d2y, d2y, thresh));

        tc0 = SimdF::max(tc0, zero);
        tc1 = SimdF::max(tc1, zero);
        tc2 = SimdF::max(tc2, zero);

        tc0 *= tc0; tc0 *= tc0;
        tc1 *= tc1; tc1 *= tc1;
        tc2 *= tc2; tc2 *= tc2;

        SimdF g0 = NoiseLib::Base::grad2_simd(vh0, d0x, d0y);
        SimdF g1 = NoiseLib::Base::grad2_simd(vh1, d1x, d1y);
        SimdF g2 = NoiseLib::Base::grad2_simd(vh2, d2x, d2y);

        SimdF vn = tc0 * g0;
        vn = SimdF::mul_add(tc1, g1, vn);
        vn = SimdF::mul_add(tc2, g2, vn);

        return vn * SimdF(70.0f);
    }

    SimdF simd3D(const SimdF& vpx, const SimdF& vpy, const SimdF& vpz, const SimdI& vseed)
    {
        constexpr float SCALAR_F3 = 1.0f / 3.0f;
        constexpr float SCALAR_G3 = 1.0f / 6.0f;

        const SimdF F3 = SimdF(SCALAR_F3);
        const SimdF G3 = SimdF(SCALAR_G3);
        const SimdF G3x2 = SimdF(SCALAR_G3 * 2.0f);
        const SimdF G3x3 = SimdF(SCALAR_G3 * 3.0f);
        const SimdF oneF = SimdF(1.0f);
        const SimdI oneI = SimdI(1);
        const SimdI allOnes = SimdI(-1);
        const SimdF thresh = SimdF(0.6f);
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
        SimdI mxy = (d0x >= d0y).as_int32();
        SimdI mxz = (d0x >= d0z).as_int32();
        SimdI myz = (d0y >= d0z).as_int32();

        SimdI notMxy = mxy ^ allOnes;
        SimdI notMxz = mxz ^ allOnes;
        SimdI notMyz = myz ^ allOnes;

        SimdI vi1x = (mxy & mxz) & oneI;
        SimdI vi1y = SimdI::bitwise_andnot(mxy, myz) & oneI;
        SimdI vi1z = oneI - vi1x - vi1y;

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
        SimdF tc0 = SimdF::neg_mul_add(d0x, d0x, SimdF::neg_mul_add(d0y, d0y, SimdF::neg_mul_add(d0z, d0z, thresh)));
        SimdF tc1 = SimdF::neg_mul_add(d1x, d1x, SimdF::neg_mul_add(d1y, d1y, SimdF::neg_mul_add(d1z, d1z, thresh)));
        SimdF tc2 = SimdF::neg_mul_add(d2x, d2x, SimdF::neg_mul_add(d2y, d2y, SimdF::neg_mul_add(d2z, d2z, thresh)));
        SimdF tc3 = SimdF::neg_mul_add(d3x, d3x, SimdF::neg_mul_add(d3y, d3y, SimdF::neg_mul_add(d3z, d3z, thresh)));

        tc0 = SimdF::max(tc0, zero);
        tc1 = SimdF::max(tc1, zero);
        tc2 = SimdF::max(tc2, zero);
        tc3 = SimdF::max(tc3, zero);

        tc0 *= tc0; tc0 *= tc0;
        tc1 *= tc1; tc1 *= tc1;
        tc2 *= tc2; tc2 *= tc2;
        tc3 *= tc3; tc3 *= tc3;

        SimdF g0 = NoiseLib::Base::grad3_simd(vh0, d0x, d0y, d0z);
        SimdF g1 = NoiseLib::Base::grad3_simd(vh1, d1x, d1y, d1z);
        SimdF g2 = NoiseLib::Base::grad3_simd(vh2, d2x, d2y, d2z);
        SimdF g3 = NoiseLib::Base::grad3_simd(vh3, d3x, d3y, d3z);

        SimdF vn = tc0 * g0;
        vn = SimdF::mul_add(tc1, g1, vn);
        vn = SimdF::mul_add(tc2, g2, vn);
        vn = SimdF::mul_add(tc3, g3, vn);

        return vn * SimdF(32.0f);
    }
}