#pragma once
#include "OpenGLWrappers/Texture.h"
#include <string>
#include <vector>
#include <filesystem>

namespace TextureLoader
{
    struct TextureLoadParams
    {
        int desiredChannels = 4;
        bool createMipmaps = false;
    };

    void createTexture2DFromImage(
        Texture& texture,
        const std::filesystem::path& texturePath,
        const TextureLoadParams& params = TextureLoadParams()
    );

    void createTextureArrayFromImages(
        Texture& texture,
        const std::filesystem::path& texturesFolderPath,
        const std::vector<std::string>& textureNames,
        const TextureLoadParams& params = TextureLoadParams()
    );

    void createTexture3DFromFloatData(
        Texture& texture,
        const std::vector<float>& data,
        int width, int height, int depth,
        const TextureLoadParams& params = TextureLoadParams()
    );
}