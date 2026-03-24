#include "Perlin.h"

namespace NoiseLib::Perlin
{
    // =========================================================================
    // Scalar
    // =========================================================================

    template<typename T>
    static inline T fade(const T& t) noexcept
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    static inline float lerp(float a, float b, float t) noexcept
    {
        return b * t + (1.0f - t) * a;
    }

    static float grad3(uint32_t hash, const glm::vec3& p)
    {
        static const glm::vec3 gradients[16] = {
            // Unique
            {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
            {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
            {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1},

            // Repeated
            { 1,1,0 },{-1,1,0},{1,-1,0},{-1,-1,0},
        };

        return glm::dot(gradients[hash & 15], p);
    }

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

    float scalar2D(const glm::vec2& p, int seed)
    {
        glm::vec2 pi_f = floor(p);
        glm::ivec2 pi = glm::ivec2(pi_f);

        glm::vec2 pf = p - pi_f;

        glm::vec2 f = fade(pf);

        glm::ivec2 c00 = glm::ivec2(pi.x, pi.y);
        glm::ivec2 c10 = glm::ivec2(pi.x + 1, pi.y);
        glm::ivec2 c01 = glm::ivec2(pi.x, pi.y + 1);
        glm::ivec2 c11 = glm::ivec2(pi.x + 1, pi.y + 1);

        float n00 = grad2(NoiseLib::Base::hash_int2(c00) + seed, pf);
        float n10 = grad2(NoiseLib::Base::hash_int2(c10) + seed, pf - glm::vec2(1, 0));
        float n01 = grad2(NoiseLib::Base::hash_int2(c01) + seed, pf - glm::vec2(0, 1));
        float n11 = grad2(NoiseLib::Base::hash_int2(c11) + seed, pf - glm::vec2(1, 1));

        float x0 = lerp(n00, n10, f.x);
        float x1 = lerp(n01, n11, f.x);

        return lerp(x0, x1, f.y);
    }

    float scalar3D(const glm::vec3& p, int seed)
    {
        glm::vec3 pi_f = glm::floor(p);
        glm::ivec3 pi = pi_f;

        glm::vec3 pf = p - pi_f;

        glm::vec3 f = fade(pf);

        // Calculate coords
        glm::ivec3 c000 = pi;
        glm::ivec3 c001 = pi + glm::ivec3(0, 0, 1);
        glm::ivec3 c010 = pi + glm::ivec3(0, 1, 0);
        glm::ivec3 c011 = pi + glm::ivec3(0, 1, 1);
        glm::ivec3 c100 = pi + glm::ivec3(1, 0, 0);
        glm::ivec3 c101 = pi + glm::ivec3(1, 0, 1);
        glm::ivec3 c110 = pi + glm::ivec3(1, 1, 0);
        glm::ivec3 c111 = pi + glm::ivec3(1, 1, 1);

        // Calculate gradients at cube corners
        float n000 = grad3(NoiseLib::Base::hash_int3(c000) + seed, pf);
        float n001 = grad3(NoiseLib::Base::hash_int3(c001) + seed, pf - glm::vec3(0, 0, 1));
        float n010 = grad3(NoiseLib::Base::hash_int3(c010) + seed, pf - glm::vec3(0, 1, 0));
        float n011 = grad3(NoiseLib::Base::hash_int3(c011) + seed, pf - glm::vec3(0, 1, 1));
        float n100 = grad3(NoiseLib::Base::hash_int3(c100) + seed, pf - glm::vec3(1, 0, 0));
        float n101 = grad3(NoiseLib::Base::hash_int3(c101) + seed, pf - glm::vec3(1, 0, 1));
        float n110 = grad3(NoiseLib::Base::hash_int3(c110) + seed, pf - glm::vec3(1, 1, 0));
        float n111 = grad3(NoiseLib::Base::hash_int3(c111) + seed, pf - glm::vec3(1, 1, 1));

        // Interpolate
        float x00 = lerp(n000, n100, f.x);
        float x01 = lerp(n001, n101, f.x);
        float x10 = lerp(n010, n110, f.x);
        float x11 = lerp(n011, n111, f.x);

        float y0 = lerp(x00, x10, f.y);
        float y1 = lerp(x01, x11, f.y);

        return lerp(y0, y1, f.z);
    }

    float scalar2DSeamless(const glm::vec2& p, int period, int seed)
    {
        glm::vec2 pi_f = floor(p);
        glm::ivec2 pi = glm::ivec2(pi_f);

        glm::vec2 pf = p - pi_f;

        glm::vec2 f = fade(pf);

        glm::ivec2 c00 = NoiseLib::Base::wrap(glm::ivec2(pi.x    , pi.y    ), period);
        glm::ivec2 c10 = NoiseLib::Base::wrap(glm::ivec2(pi.x + 1, pi.y    ), period);
        glm::ivec2 c01 = NoiseLib::Base::wrap(glm::ivec2(pi.x    , pi.y + 1), period);
        glm::ivec2 c11 = NoiseLib::Base::wrap(glm::ivec2(pi.x + 1, pi.y + 1), period);

        float n00 = grad2(NoiseLib::Base::hash_int2(c00) + seed, pf);
        float n10 = grad2(NoiseLib::Base::hash_int2(c10) + seed, pf - glm::vec2(1, 0));
        float n01 = grad2(NoiseLib::Base::hash_int2(c01) + seed, pf - glm::vec2(0, 1));
        float n11 = grad2(NoiseLib::Base::hash_int2(c11) + seed, pf - glm::vec2(1, 1));

        float x0 = lerp(n00, n10, f.x);
        float x1 = lerp(n01, n11, f.x);

        return lerp(x0, x1, f.y);
    }

    float scalar3DSeamless(const glm::vec3& p, int period, int seed)
    {
        glm::vec3 pi_f = glm::floor(p);
        glm::ivec3 pi = pi_f;

        glm::vec3 pf = p - pi_f;

        glm::vec3 f = fade(pf);

        // Calculate coords
        glm::ivec3 c000 = NoiseLib::Base::wrap(pi                      , period);
        glm::ivec3 c001 = NoiseLib::Base::wrap(pi + glm::ivec3(0, 0, 1), period);
        glm::ivec3 c010 = NoiseLib::Base::wrap(pi + glm::ivec3(0, 1, 0), period);
        glm::ivec3 c011 = NoiseLib::Base::wrap(pi + glm::ivec3(0, 1, 1), period);
        glm::ivec3 c100 = NoiseLib::Base::wrap(pi + glm::ivec3(1, 0, 0), period);
        glm::ivec3 c101 = NoiseLib::Base::wrap(pi + glm::ivec3(1, 0, 1), period);
        glm::ivec3 c110 = NoiseLib::Base::wrap(pi + glm::ivec3(1, 1, 0), period);
        glm::ivec3 c111 = NoiseLib::Base::wrap(pi + glm::ivec3(1, 1, 1), period);

        // Calculate gradients at cube corners
        float n000 = grad3(NoiseLib::Base::hash_int3(c000) + seed, pf);
        float n001 = grad3(NoiseLib::Base::hash_int3(c001) + seed, pf - glm::vec3(0, 0, 1));
        float n010 = grad3(NoiseLib::Base::hash_int3(c010) + seed, pf - glm::vec3(0, 1, 0));
        float n011 = grad3(NoiseLib::Base::hash_int3(c011) + seed, pf - glm::vec3(0, 1, 1));
        float n100 = grad3(NoiseLib::Base::hash_int3(c100) + seed, pf - glm::vec3(1, 0, 0));
        float n101 = grad3(NoiseLib::Base::hash_int3(c101) + seed, pf - glm::vec3(1, 0, 1));
        float n110 = grad3(NoiseLib::Base::hash_int3(c110) + seed, pf - glm::vec3(1, 1, 0));
        float n111 = grad3(NoiseLib::Base::hash_int3(c111) + seed, pf - glm::vec3(1, 1, 1));

        // Interpolate
        float x00 = lerp(n000, n100, f.x);
        float x01 = lerp(n001, n101, f.x);
        float x10 = lerp(n010, n110, f.x);
        float x11 = lerp(n011, n111, f.x);

        float y0 = lerp(x00, x10, f.y);
        float y1 = lerp(x01, x11, f.y);

        return lerp(y0, y1, f.z);
    }

    // =========================================================================
    // SIMD
    // =========================================================================

    static SimdF grad2_simd(const SimdI& hash, const SimdF& dx, const SimdF& dy)
    {
        const SimdI one = SimdI::fill_lanes_with_value(1);
        const SimdI two = SimdI::fill_lanes_with_value(2);
        const SimdI four = SimdI::fill_lanes_with_value(4);
        const SimdF SIGN = SimdF::fill_lanes_with_value(-0.0f);

        SimdF mbit0 = (((hash & one) == one)).as_float();
        SimdF mbit1 = (((hash & two) == two)).as_float();
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

        // Extract bits 0-3
        SimdI bit0 = hash & one;
        SimdI bit1 = (hash >> 1) & one;
        SimdI bit2 = (hash >> 2) & one;
        SimdI bit3 = (hash >> 3) & one;

        SimdF mbit0 = (bit0 == one).as_float();  // sign of first component
        SimdF mbit1 = (bit1 == one).as_float();  // sign of second component
        SimdF mbit2 = (bit2 == one).as_float();  // axis selector

        // The 16 gradients fall into three groups (matching grad3 scalar):
        //   A (bit3==bit2, i.e. indices 0-3 and 12-15): (±1, ±1,  0) -> ±dx ± dy
        //   B (bit3=0, bit2=1, i.e. indices  4-7):      (±1,  0, ±1) -> ±dx ± dz
        //   C (bit3=1, bit2=0, i.e. indices 8-11):      ( 0, ±1, ±1) -> ±dy ± dz
        SimdF ga = SimdF::blendv(dx, dx ^ SIGN, mbit0) + SimdF::blendv(dy, dy ^ SIGN, mbit1);
        SimdF gb = SimdF::blendv(dx, dx ^ SIGN, mbit0) + SimdF::blendv(dz, dz ^ SIGN, mbit1);
        SimdF gc = SimdF::blendv(dy, dy ^ SIGN, mbit0) + SimdF::blendv(dz, dz ^ SIGN, mbit1);

        // mask_bc: set when bit3 != bit2 (groups B or C)
        SimdF mask_bc = ((bit2 ^ bit3) == one).as_float();
        // Within BC: bit2=1 -> group B, bit2=0 -> group C
        SimdF groupBC = SimdF::blendv(gc, gb, mbit2);

        return SimdF::blendv(ga, groupBC, mask_bc);
    }

    static SimdF fade_simd(const SimdF& t)
    {
        // t^3 * (t * (6t - 15) + 10)
        const SimdF six = SimdF::fill_lanes_with_value(6.0f);
        const SimdF fifteen = SimdF::fill_lanes_with_value(15.0f);
        const SimdF ten = SimdF::fill_lanes_with_value(10.0f);
        
        SimdF t3 = t * t * t;
        return t3 * (t * (t * six - fifteen) + ten);
    }

    SimdF simd2D(const SimdF& vpx, const SimdF& vpy, const SimdI& vseed)
    {
        const SimdF oneF = SimdF::fill_lanes_with_value(1.0f);
        const SimdI oneI = SimdI::fill_lanes_with_value(1);

        // Integer cell and fractional offset
        SimdF pfx = SimdF::floor(vpx);
        SimdF pfy = SimdF::floor(vpy);
        SimdI pix = pfx.to_int32();
        SimdI piy = pfy.to_int32();
        SimdF pfracx = vpx - pfx;
        SimdF pfracy = vpy - pfy;

        // Fade curves
        SimdF fx = fade_simd(pfracx);
        SimdF fy = fade_simd(pfracy);

        // Four corner hashes — z is always 0 for 2D
        SimdI pix1 = pix + oneI;
        SimdI piy1 = piy + oneI;
        SimdI vh00 = NoiseLib::Base::hash_int2_simd(pix,  piy ) + vseed;
        SimdI vh10 = NoiseLib::Base::hash_int2_simd(pix1, piy ) + vseed;
        SimdI vh01 = NoiseLib::Base::hash_int2_simd(pix,  piy1) + vseed;
        SimdI vh11 = NoiseLib::Base::hash_int2_simd(pix1, piy1) + vseed;

        // Gradient contributions
        SimdF n00 = grad2_simd(vh00, pfracx, pfracy);
        SimdF n10 = grad2_simd(vh10, pfracx - oneF, pfracy);
        SimdF n01 = grad2_simd(vh01, pfracx, pfracy - oneF);
        SimdF n11 = grad2_simd(vh11, pfracx - oneF, pfracy - oneF);

        // Bilinear interpolation
        SimdF x0 = n00 + fx * (n10 - n00);
        SimdF x1 = n01 + fx * (n11 - n01);
        return x0 + fy * (x1 - x0);
    }

    SimdF simd3D(const SimdF& vpx, const SimdF& vpy, const SimdF& vpz, const SimdI& vseed)
    {
        const SimdF oneF = SimdF::fill_lanes_with_value(1.0f);
        const SimdI oneI = SimdI::fill_lanes_with_value(1);

        // Integer cell and fractional offsets
        SimdF pfx = SimdF::floor(vpx);
        SimdF pfy = SimdF::floor(vpy);
        SimdF pfz = SimdF::floor(vpz);
        SimdI pix = pfx.to_int32();
        SimdI piy = pfy.to_int32();
        SimdI piz = pfz.to_int32();
        SimdF pfracx = vpx - pfx;
        SimdF pfracy = vpy - pfy;
        SimdF pfracz = vpz - pfz;

        // Fade curves
        SimdF fx = fade_simd(pfracx);
        SimdF fy = fade_simd(pfracy);
        SimdF fz = fade_simd(pfracz);

        // Wrapped corner coordinates
        SimdI pix0 = pix;
        SimdI pix1 = pix + oneI;
        SimdI piy0 = piy;
        SimdI piy1 = piy + oneI;
        SimdI piz0 = piz;
        SimdI piz1 = piz + oneI;

        // Eight corner hashes
        SimdI vh000 = NoiseLib::Base::hash_int3_simd(pix0, piy0, piz0) + vseed;
        SimdI vh100 = NoiseLib::Base::hash_int3_simd(pix1, piy0, piz0) + vseed;
        SimdI vh010 = NoiseLib::Base::hash_int3_simd(pix0, piy1, piz0) + vseed;
        SimdI vh110 = NoiseLib::Base::hash_int3_simd(pix1, piy1, piz0) + vseed;
        SimdI vh001 = NoiseLib::Base::hash_int3_simd(pix0, piy0, piz1) + vseed;
        SimdI vh101 = NoiseLib::Base::hash_int3_simd(pix1, piy0, piz1) + vseed;
        SimdI vh011 = NoiseLib::Base::hash_int3_simd(pix0, piy1, piz1) + vseed;
        SimdI vh111 = NoiseLib::Base::hash_int3_simd(pix1, piy1, piz1) + vseed;

        // Gradient contributions at each corner
        SimdF n000 = grad3_simd(vh000, pfracx, pfracy, pfracz);
        SimdF n100 = grad3_simd(vh100, pfracx - oneF, pfracy, pfracz);
        SimdF n010 = grad3_simd(vh010, pfracx, pfracy - oneF, pfracz);
        SimdF n110 = grad3_simd(vh110, pfracx - oneF, pfracy - oneF, pfracz);
        SimdF n001 = grad3_simd(vh001, pfracx, pfracy, pfracz - oneF);
        SimdF n101 = grad3_simd(vh101, pfracx - oneF, pfracy, pfracz - oneF);
        SimdF n011 = grad3_simd(vh011, pfracx, pfracy - oneF, pfracz - oneF);
        SimdF n111 = grad3_simd(vh111, pfracx - oneF, pfracy - oneF, pfracz - oneF);

        // Trilinear interpolation
        SimdF x00 = n000 + fx * (n100 - n000);
        SimdF x10 = n010 + fx * (n110 - n010);
        SimdF x01 = n001 + fx * (n101 - n001);
        SimdF x11 = n011 + fx * (n111 - n011);

        SimdF y0 = x00 + fy * (x10 - x00);
        SimdF y1 = x01 + fy * (x11 - x01);

        return y0 + fz * (y1 - y0);
    }

    SimdF simd2DSeamless(const SimdF& vpx, const SimdF& vpy, const SimdI& vperiod, const SimdI& vseed)
    {
        const SimdF oneF = SimdF::fill_lanes_with_value(1.0f);
        const SimdI oneI = SimdI::fill_lanes_with_value(1);

        // Integer cell and fractional offset
        SimdF pfx = SimdF::floor(vpx);
        SimdF pfy = SimdF::floor(vpy);
        SimdI pix = pfx.to_int32();
        SimdI piy = pfy.to_int32();
        SimdF pfracx = vpx - pfx;
        SimdF pfracy = vpy - pfy;

        // Fade curves
        SimdF fx = fade_simd(pfracx);
        SimdF fy = fade_simd(pfracy);

        // Four corner hashes — z is always 0 for 2D
        SimdI pix1 = pix + oneI;
        SimdI piy1 = piy + oneI;
        SimdI vh00 = NoiseLib::Base::hash_int2_simd(NoiseLib::Base::wrap_simd(pix, vperiod), NoiseLib::Base::wrap_simd(piy, vperiod)) + vseed;
        SimdI vh10 = NoiseLib::Base::hash_int2_simd(NoiseLib::Base::wrap_simd(pix1, vperiod), NoiseLib::Base::wrap_simd(piy, vperiod)) + vseed;
        SimdI vh01 = NoiseLib::Base::hash_int2_simd(NoiseLib::Base::wrap_simd(pix, vperiod), NoiseLib::Base::wrap_simd(piy1, vperiod)) + vseed;
        SimdI vh11 = NoiseLib::Base::hash_int2_simd(NoiseLib::Base::wrap_simd(pix1, vperiod), NoiseLib::Base::wrap_simd(piy1, vperiod)) + vseed;

        // Gradient contributions
        SimdF n00 = grad2_simd(vh00, pfracx, pfracy);
        SimdF n10 = grad2_simd(vh10, pfracx - oneF, pfracy);
        SimdF n01 = grad2_simd(vh01, pfracx, pfracy - oneF);
        SimdF n11 = grad2_simd(vh11, pfracx - oneF, pfracy - oneF);

        // Bilinear interpolation
        SimdF x0 = n00 + fx * (n10 - n00);
        SimdF x1 = n01 + fx * (n11 - n01);
        return x0 + fy * (x1 - x0);
    }

    SimdF simd3DSeamless(const SimdF& vpx, const SimdF& vpy, const SimdF& vpz, const SimdI& vperiod, const SimdI& vseed)
    {
        const SimdF oneF = SimdF::fill_lanes_with_value(1.0f);
        const SimdI oneI = SimdI::fill_lanes_with_value(1);

        // Integer cell and fractional offsets
        SimdF pfx = SimdF::floor(vpx);
        SimdF pfy = SimdF::floor(vpy);
        SimdF pfz = SimdF::floor(vpz);
        SimdI pix = pfx.to_int32();
        SimdI piy = pfy.to_int32();
        SimdI piz = pfz.to_int32();
        SimdF pfracx = vpx - pfx;
        SimdF pfracy = vpy - pfy;
        SimdF pfracz = vpz - pfz;

        // Fade curves
        SimdF fx = fade_simd(pfracx);
        SimdF fy = fade_simd(pfracy);
        SimdF fz = fade_simd(pfracz);

        // Wrapped corner coordinates
        SimdI pix0 = NoiseLib::Base::wrap_simd(pix, vperiod);
        SimdI pix1 = NoiseLib::Base::wrap_simd(pix + oneI, vperiod);
        SimdI piy0 = NoiseLib::Base::wrap_simd(piy, vperiod);
        SimdI piy1 = NoiseLib::Base::wrap_simd(piy + oneI, vperiod);
        SimdI piz0 = NoiseLib::Base::wrap_simd(piz, vperiod);
        SimdI piz1 = NoiseLib::Base::wrap_simd(piz + oneI, vperiod);

        // Eight corner hashes
        SimdI vh000 = NoiseLib::Base::hash_int3_simd(pix0, piy0, piz0) + vseed;
        SimdI vh100 = NoiseLib::Base::hash_int3_simd(pix1, piy0, piz0) + vseed;
        SimdI vh010 = NoiseLib::Base::hash_int3_simd(pix0, piy1, piz0) + vseed;
        SimdI vh110 = NoiseLib::Base::hash_int3_simd(pix1, piy1, piz0) + vseed;
        SimdI vh001 = NoiseLib::Base::hash_int3_simd(pix0, piy0, piz1) + vseed;
        SimdI vh101 = NoiseLib::Base::hash_int3_simd(pix1, piy0, piz1) + vseed;
        SimdI vh011 = NoiseLib::Base::hash_int3_simd(pix0, piy1, piz1) + vseed;
        SimdI vh111 = NoiseLib::Base::hash_int3_simd(pix1, piy1, piz1) + vseed;

        // Gradient contributions at each corner
        SimdF n000 = grad3_simd(vh000, pfracx, pfracy, pfracz);
        SimdF n100 = grad3_simd(vh100, pfracx - oneF, pfracy, pfracz);
        SimdF n010 = grad3_simd(vh010, pfracx, pfracy - oneF, pfracz);
        SimdF n110 = grad3_simd(vh110, pfracx - oneF, pfracy - oneF, pfracz);
        SimdF n001 = grad3_simd(vh001, pfracx, pfracy, pfracz - oneF);
        SimdF n101 = grad3_simd(vh101, pfracx - oneF, pfracy, pfracz - oneF);
        SimdF n011 = grad3_simd(vh011, pfracx, pfracy - oneF, pfracz - oneF);
        SimdF n111 = grad3_simd(vh111, pfracx - oneF, pfracy - oneF, pfracz - oneF);

        // Trilinear interpolation
        SimdF x00 = n000 + fx * (n100 - n000);
        SimdF x10 = n010 + fx * (n110 - n010);
        SimdF x01 = n001 + fx * (n101 - n001);
        SimdF x11 = n011 + fx * (n111 - n011);

        SimdF y0 = x00 + fy * (x10 - x00);
        SimdF y1 = x01 + fy * (x11 - x01);

        return y0 + fz * (y1 - y0);
    }
}