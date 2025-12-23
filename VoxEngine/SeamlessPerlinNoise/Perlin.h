#pragma once
#include <vector>

// I could use FastNoise2, which I already have in this project, but I did/t figure out how to make it seamless
namespace SeamlessPerlinNoise
{
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
    );
}
