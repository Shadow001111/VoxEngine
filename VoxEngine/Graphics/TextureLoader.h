#pragma once
#include "OpenGLWrappers/OpenGL_Texture.h"
#include <string>
#include <vector>
#include <filesystem>

namespace TextureLoader
{
    std::vector<unsigned char> createUndefinedTexture(int textureSize, int channels);

    struct TextureParams
    {
        GLenum minFilter = GL_NEAREST;
        GLenum magFilter = GL_NEAREST;
        GLenum wrapMode = GL_CLAMP_TO_EDGE;
        int desiredChannels = 4;
        bool createMipmaps = false;
    };

    void createAndLoadTexture2D(
        OpenGL_Texture& texture,
        const std::filesystem::path& texturePath,
        int textureSize,
        const TextureParams& params = TextureParams()
    );

    void createAndLoadTextureArray(
        OpenGL_Texture& texture,
        const std::filesystem::path& texturesFolderPath,
        const std::vector<std::string>& textureNames,
        int textureSize,
        const TextureParams& params = TextureParams()
    );

    void createAndLoadTexture3DFromFloatData(
        OpenGL_Texture& texture,
        const std::vector<float>& data,
        int textureSize,
        const TextureParams& params = TextureParams()
    );
}