#pragma once
#include "OpenGLWrappers/OpenGL_Texture.h"
#include <string>
#include <vector>
#include <filesystem>

namespace TextureLoader
{
    std::vector<unsigned char> createUndefinedTexture(int width, int height, int channels);

    struct TextureParams
    {
        GLenum minFilter = GL_NEAREST;
        GLenum magFilter = GL_NEAREST;
        GLenum wrapMode = GL_CLAMP_TO_EDGE;
        int desiredChannels = 4;
        bool createMipmaps = false;
    };

    void createTexture2DFromImage(
        OpenGL_Texture& texture,
        const std::filesystem::path& texturePath,
        const TextureParams& params = TextureParams()
    );

    void createTextureArrayFromImages(
        OpenGL_Texture& texture,
        const std::filesystem::path& texturesFolderPath,
        const std::vector<std::string>& textureNames,
        const TextureParams& params = TextureParams()
    );

    void createTexture3DFromFloatData(
        OpenGL_Texture& texture,
        const std::vector<float>& data,
        int width, int height, int depth,
        const TextureParams& params = TextureParams()
    );
}