#pragma once

namespace SeamlessPerlinNoise
{
    void generatePerlinNoise3D(
        float* out,
        int resolutionX,
        int resolutionY,
        int resolutionZ,
        float perlinSize,
        int perlinOctaves,
        float perlinLacunarity,
        bool seamless,
        int seed
    );
}
