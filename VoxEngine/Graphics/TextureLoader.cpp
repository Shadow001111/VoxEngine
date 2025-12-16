#include "TextureLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <iostream>
#include <cmath>

namespace fs = std::filesystem;

namespace TextureLoader
{
    std::vector<unsigned char> createUndefinedTexture(int textureSize, int channels)
    {
        std::vector<unsigned char> undefinedTexture(textureSize * textureSize * channels);

        if (channels == 4)
        {
            size_t index = 0;
            for (int y = 0; y < textureSize; y++)
            {
                bool hy = y > (textureSize >> 1);
                for (int x = 0; x < textureSize; x++)
                {
                    bool hx = x > (textureSize >> 1);
                    bool hxy = hx ^ hy;

                    unsigned char r, g, b, a;
                    r = hxy ? 255 : 0;
                    g = 0;
                    b = r;
                    a = 255;

                    undefinedTexture[index] = r;
                    undefinedTexture[index + 1] = g;
                    undefinedTexture[index + 2] = b;
                    undefinedTexture[index + 3] = a;
                    index += 4;
                }
            }
        }
        else // channels == 3
        {
            size_t index = 0;
            for (int y = 0; y < textureSize; y++)
            {
                bool hy = y > (textureSize >> 1);
                for (int x = 0; x < textureSize; x++)
                {
                    bool hx = x > (textureSize >> 1);
                    bool hxy = hx ^ hy;

                    unsigned char r, g, b;
                    r = hxy ? 255 : 0;
                    g = 0;
                    b = r;

                    undefinedTexture[index] = r;
                    undefinedTexture[index + 1] = g;
                    undefinedTexture[index + 2] = b;
                    index += 3;
                }
            }
        }

        return undefinedTexture;
    }

    void createAndLoadTexture2D(OpenGL_Texture& texture, const std::filesystem::path& texturePath, int textureSize, const TextureParams& params)
    {
        if (params.desiredChannels != 3 && params.desiredChannels != 4)
        {
            std::cerr << "[TextureLoader]: Only 3 (RGB) or 4 (RGBA) channels are supported. Got: " << params.desiredChannels << std::endl;
            return;
        }

        if (!fs::exists(texturePath) || !fs::is_regular_file(texturePath))
        {
            std::cerr << "[TextureLoader]: Texture file is not found " << texturePath << std::endl;
            return;
        }

        const int mipmapLevels = 1 + (params.createMipmaps ? static_cast<int>(std::ceil(std::log2(textureSize))) : 0);

        std::vector<unsigned char> undefinedTexture = createUndefinedTexture(textureSize, params.desiredChannels);

        GLenum internalFormat = params.desiredChannels == 4 ? GL_RGBA8 : GL_RGB8;
        GLenum format = params.desiredChannels == 4 ? GL_RGBA : GL_RGB;

        texture.create2D(textureSize, textureSize, internalFormat, format, GL_UNSIGNED_BYTE, mipmapLevels);
        texture.bind();

        texture.setParameters(
            params.minFilter,
            params.magFilter,
            params.wrapMode,
            params.wrapMode,
            params.wrapMode);

        stbi_set_flip_vertically_on_load(true);

        int width, height, channels;
        unsigned char* data = stbi_load(texturePath.string().c_str(), &width, &height, &channels, params.desiredChannels);

        if (!data)
        {
            std::cerr << "[TextureLoader]: Failed to load texture: " << texturePath << "\n";
            // Upload fallback texture
            texture.uploadSubData(undefinedTexture.data(), 0, 0, 0,
                textureSize, textureSize, 1, 0);
            return;;
        }

        if (width != textureSize || height != textureSize)
        {
            std::cerr << "[TextureLoader]: Texture " << texturePath.stem()
                << " is not " << textureSize << "x" << textureSize
                << " (" << width << "x" << height << ")\n";
            stbi_image_free(data);
            // Upload fallback texture
            texture.uploadSubData(undefinedTexture.data(), 0, 0, 0,
                textureSize, textureSize, 1, 0);
            return;
        }

        // Upload the actual texture data
        texture.uploadSubData(data, 0, 0, 0, textureSize, textureSize, 1, 0);

        stbi_image_free(data);
    }

    void createAndLoadTextureArray(
        OpenGL_Texture& texture,
        const fs::path& texturesFolderPath,
        const std::vector<std::string>& textureNames,
        int textureSize,
        const TextureParams& params
    )
    {
        if (params.desiredChannels != 3 && params.desiredChannels != 4)
        {
            std::cerr << "[TextureLoader]: Only 3 (RGB) or 4 (RGBA) channels are supported. Got: " << params.desiredChannels << std::endl;
            return;
        }

        if (!fs::exists(texturesFolderPath))
        {
            std::cerr << "[TextureLoader]: Textures folder is not found for path " << texturesFolderPath << std::endl;
            return;
        }

        const size_t layerCount = textureNames.size();
        const int mipmapLevels = 1 + (params.createMipmaps ? static_cast<int>(std::ceil(std::log2(textureSize))) : 0);

        std::vector<unsigned char> undefinedTexture = createUndefinedTexture(textureSize, params.desiredChannels);

        GLenum internalFormat = params.desiredChannels == 4 ? GL_RGBA8 : GL_RGB8;
        GLenum format = params.desiredChannels == 4 ? GL_RGBA : GL_RGB;

        // Create the texture array using OpenGL_Texture
        texture.create2DArray(textureSize, textureSize, static_cast<int>(layerCount),
            internalFormat, format, GL_UNSIGNED_BYTE, mipmapLevels);
        texture.bind();

        // Set texture parameters
        texture.setParameters(
            params.minFilter,
            params.magFilter,
            params.wrapMode,
            params.wrapMode,
            params.wrapMode);

        // Load textures
        stbi_set_flip_vertically_on_load(true);

        for (size_t i = 0; i < layerCount; ++i)
        {
            fs::path fullPath = texturesFolderPath / (textureNames[i] + ".png");
            int width, height, channels;
            unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &channels, params.desiredChannels);

            if (!data)
            {
                std::cerr << "[TextureLoader]: Failed to load texture: " << fullPath << "\n";
                // Upload fallback texture
                texture.uploadSubData(undefinedTexture.data(), 0, 0, static_cast<int>(i),
                    textureSize, textureSize, 1, 0);
                continue;
            }

            if (width != textureSize || height != textureSize)
            {
                std::cerr << "[TextureLoader]: Texture " << textureNames[i]
                    << " is not " << textureSize << "x" << textureSize
                    << " (" << width << "x" << height << ")\n";
                stbi_image_free(data);
                // Upload fallback texture
                texture.uploadSubData(undefinedTexture.data(), 0, 0, static_cast<int>(i),
                    textureSize, textureSize, 1, 0);
                continue;
            }

            // Upload the actual texture data
            texture.uploadSubData(data, 0, 0, static_cast<int>(i),
                textureSize, textureSize, 1, 0);

            stbi_image_free(data);
        }

        // Generate mipmaps
        if (params.createMipmaps)
        {
            texture.generateMipmaps();
        }
    }
}