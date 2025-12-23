#include "Perlin.h"
#include <glm/glm.hpp>

namespace SeamlessPerlinNoise
{
    uint32_t hash_int3(const glm::ivec3& v)
    {
        uint32_t h = 0xdeadbeefu;
        h ^= v.x + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= v.y + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= v.z + 0x9e3779b9u + (h << 6) + (h >> 2);

        h ^= h >> 16;
        h *= 0x85ebca6bu;
        h ^= h >> 13;
        h *= 0xc2b2ae35u;
        h ^= h >> 16;
        return h;
    }

    uint32_t hash_uint(uint32_t v)
    {
        uint32_t h = v;
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return h;
    }

    glm::vec3 rand_vector(uint32_t hash)
    {
        uint32_t x = hash_uint(hash ^ 0xA53C9A1F);
        uint32_t y = hash_uint(hash ^ 0xC2B2AE35);
        uint32_t z = hash_uint(hash ^ 0x27D4EB2F);
        return glm::vec3(x, y, z) / 4294967296.0f;
    }

    float dot(const glm::vec3& a, const glm::vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    glm::vec3 fade(const glm::vec3& t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    float lerp(float a, float b, float t) {
        return b * t + (1.0f - t) * a;
    }

    glm::vec3 floor(const glm::vec3& v)
    {
        return {
            floorf(v.x),
            floorf(v.y),
            floorf(v.z)
        };
    }

    // Grad function
    float grad(uint32_t hash, const glm::vec3& p)
    {
        uint32_t h = hash & 15;
        return dot(rand_vector(h) * 2.0f - 1.0f, p);
    }

    void generatePerlinNoise3D(
        std::vector<float>& out,
        int resolutionX,
        int resolutionY,
        int resolutionZ,
        float perlinSize,
        int perlinOctaves,
        float perlinLacunarity,
        bool seamless,
        int seed
    )
    {
        // Prepare vector
        out.resize(resolutionX * resolutionY * resolutionZ);

        // Calculate grid resolution
        float grid_resolution = std::floor(1.0f / perlinSize);

        size_t index = 0; // x + y * resX + z * resX * resY
        for (int z = 0; z < resolutionZ; z++)
        {
            for (int y = 0; y < resolutionY; y++)
            {
                for (int x = 0; x < resolutionX; x++)
                {
                    glm::vec3 position = glm::vec3(x, y, z) / glm::vec3(resolutionX, resolutionY, resolutionZ) * grid_resolution;

                    float sum = 0.0f;

                    for (int i = 0; i < perlinOctaves; i++)
                    {
                        int s = seed + i;
                        float attenuation = floorf(powf(perlinLacunarity, static_cast<float>(i)));
                        glm::vec3 p = position * attenuation;

                        glm::vec3 pi_f = floor(p);
                        glm::ivec3 pi = pi_f;

                        glm::vec3 pf = p - pi_f;

                        glm::vec3 f = fade(pf);

                        glm::ivec3 period = seamless ?
                            glm::ivec3(grid_resolution * static_cast<int>(attenuation)) :
                            glm::ivec3(100000000);

                        // Calculate gradients at cube corners
                        float n000 = grad(
                            hash_int3((pi + glm::ivec3(0, 0, 0)) % period) + s,
                            pf - glm::vec3(0, 0, 0)
                        );
                        float n001 = grad(
                            hash_int3((pi + glm::ivec3(0, 0, 1)) % period) + s,
                            pf - glm::vec3(0, 0, 1)
                        );
                        float n010 = grad(
                            hash_int3((pi + glm::ivec3(0, 1, 0)) % period) + s,
                            pf - glm::vec3(0, 1, 0)
                        );
                        float n011 = grad(
                            hash_int3((pi + glm::ivec3(0, 1, 1)) % period) + s,
                            pf - glm::vec3(0, 1, 1)
                        );
                        float n100 = grad(
                            hash_int3((pi + glm::ivec3(1, 0, 0)) % period) + s,
                            pf - glm::vec3(1, 0, 0)
                        );
                        float n101 = grad(
                            hash_int3((pi + glm::ivec3(1, 0, 1)) % period) + s,
                            pf - glm::vec3(1, 0, 1)
                        );
                        float n110 = grad(
                            hash_int3((pi + glm::ivec3(1, 1, 0)) % period) + s,
                            pf - glm::vec3(1, 1, 0)
                        );
                        float n111 = grad(
                            hash_int3((pi + glm::ivec3(1, 1, 1)) % period) + s,
                            pf - glm::vec3(1, 1, 1)
                        );

                        // Interpolate
                        float x00 = lerp(n000, n100, f.x);
                        float x01 = lerp(n001, n101, f.x);
                        float x10 = lerp(n010, n110, f.x);
                        float x11 = lerp(n011, n111, f.x);

                        float y0 = lerp(x00, x10, f.y);
                        float y1 = lerp(x01, x11, f.y);

                        sum += lerp(y0, y1, f.z) / attenuation;
                    }

                    // Convert from [-1, 1] to [0, 1] range
                    float output = sum * 0.5f + 0.5f;

                    out[index++] = output;
                }
            }
        }
    }
}