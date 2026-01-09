#include "TextureLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <iostream>
#include <cmath>

namespace fs = std::filesystem;

namespace TextureLoader
{
    std::vector<unsigned char> createUndefinedTexture(int width, int height, int channels)
    {
        std::vector<unsigned char> undefinedTexture(width * height * channels);

        const int halfWidth = width >> 1;
        const int halfHeight = height >> 1;

        if (channels == 4)
        {
            size_t index = 0;
            for (int y = 0; y < height; y++)
            {
                bool hy = y > halfHeight;
                for (int x = 0; x < width; x++)
                {
                    bool hx = x > halfWidth;
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
        else if (channels == 3)
        {
            size_t index = 0;
            for (int y = 0; y < height; y++)
            {
                bool hy = y > halfHeight;
                for (int x = 0; x < width; x++)
                {
                    bool hx = x > halfWidth;
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
        else if (channels == 2)
        {
            size_t index = 0;
            for (int y = 0; y < height; y++)
            {
                bool hy = y > halfHeight;
                for (int x = 0; x < width; x++)
                {
                    bool hx = x > halfWidth;
                    bool hxy = hx ^ hy;

                    unsigned char r, a;
                    r = hxy ? 255 : 0;
                    a = 255;

                    undefinedTexture[index] = r;
                    undefinedTexture[index + 1] = a;
                    index += 2;
                }
            }
        }
        else if (channels == 1)
        {
            size_t index = 0;
            for (int y = 0; y < height; y++)
            {
                bool hy = y > halfHeight;
                for (int x = 0; x < width; x++)
                {
                    bool hx = x > halfWidth;
                    bool hxy = hx ^ hy;

                    unsigned char r;
                    r = hxy ? 255 : 0;

                    undefinedTexture[index] = r;
                    index += 1;
                }
            }
        }

        return undefinedTexture;
    }

    GLenum getInternalFormat(int channels)
    {
        switch (channels)
        {
        case 1: return GL_R8;
        case 2: return GL_RG8;
        case 3: return GL_RGB8;
        case 4: return GL_RGBA8;
        default: return GL_RGBA8;
        }
    }

    GLenum getFormat(int channels)
    {
        switch (channels)
        {
        case 1: return GL_RED;
        case 2: return GL_RG;
        case 3: return GL_RGB;
        case 4: return GL_RGBA;
        default: return GL_RGBA;
        }
    }

    void createAndLoadTexture2D(
        OpenGL_Texture& texture,
        const std::filesystem::path& texturePath,
        const TextureParams& params)
    {
        if (params.desiredChannels < 1 || params.desiredChannels > 4)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture2D]: Only 1-4 channels are supported. Got: " << params.desiredChannels << "\n";
            return;
        }

        if (!fs::exists(texturePath) || !fs::is_regular_file(texturePath))
        {
            std::cerr << "[TextureLoader][createAndLoadTexture2D]: Texture file is not found " << texturePath << "\n";
            return;
        }

        stbi_set_flip_vertically_on_load(true);

        int width, height, channels;
        unsigned char* data = stbi_load(texturePath.string().c_str(), &width, &height, &channels, 0);

        if (!data)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture2D]: Failed to load texture: " << texturePath << "\n";

            // Create a default-sized undefined texture
            const int defaultSize = 16;
            std::vector<unsigned char> undefinedTexture = createUndefinedTexture(defaultSize, defaultSize, params.desiredChannels);

            const int mipmapLevels = 1 + (params.createMipmaps ? static_cast<int>(std::ceil(std::log2(defaultSize))) : 0);
            GLenum internalFormat = getInternalFormat(params.desiredChannels);
            GLenum format = getFormat(params.desiredChannels);

            texture.create2D(defaultSize, defaultSize, internalFormat, format, GL_UNSIGNED_BYTE, mipmapLevels);
            texture.bind();

            texture.setParameters(
                params.minFilter,
                params.magFilter,
                params.wrapMode,
                params.wrapMode,
                params.wrapMode);

            texture.uploadSubData(undefinedTexture.data(), 0, 0, 0,
                defaultSize, defaultSize, 1, 0);
            return;
        }

        const int mipmapLevels = 1 + (params.createMipmaps ? static_cast<int>(std::ceil(std::log2(std::max(width, height)))) : 0);

        // Create undefined texture with the actual dimensions
        std::vector<unsigned char> undefinedTexture = createUndefinedTexture(width, height, params.desiredChannels);

        GLenum internalFormat = getInternalFormat(params.desiredChannels);
        GLenum format = getFormat(params.desiredChannels);

        texture.create2D(width, height, internalFormat, format, GL_UNSIGNED_BYTE, mipmapLevels);
        texture.bind();

        texture.setParameters(
            params.minFilter,
            params.magFilter,
            params.wrapMode,
            params.wrapMode,
            params.wrapMode);

        // Convert data to desired channel format
        std::vector<unsigned char> convertedData;
        if (channels != params.desiredChannels)
        {
            convertedData.resize(width * height * params.desiredChannels);
            int srcStride = width * channels;
            int dstStride = width * params.desiredChannels;

            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    int srcIndex = y * srcStride + x * channels;
                    int dstIndex = y * dstStride + x * params.desiredChannels;

                    // Copy available channels, pad missing ones with 0 (or 255 for alpha if needed)
                    for (int c = 0; c < params.desiredChannels; c++)
                    {
                        if (c < channels)
                        {
                            // Copy existing channel
                            convertedData[dstIndex + c] = data[srcIndex + c];
                        }
                        else
                        {
                            // Pad with 0 for RGB channels, 255 for alpha if it's the 4th channel
                            if (params.desiredChannels == 4 && c == 3)
                            {
                                // If we're adding an alpha channel, default to fully opaque
                                convertedData[dstIndex + c] = 255;
                            }
                            else if (params.desiredChannels == 2 && c == 1)
                            {
                                // For 2-channel, if we need a second channel, default to 255
                                convertedData[dstIndex + c] = 255;
                            }
                            else
                            {
                                convertedData[dstIndex + c] = 0;
                            }
                        }
                    }
                }
            }
        }

        // Upload the texture data
        if (channels == params.desiredChannels)
        {
            texture.uploadSubData(data, 0, 0, 0, width, height, 1, 0);
        }
        else
        {
            texture.uploadSubData(convertedData.data(), 0, 0, 0, width, height, 1, 0);
        }

        stbi_image_free(data);
    }

    void createAndLoadTextureArray(
        OpenGL_Texture& texture,
        const fs::path& texturesFolderPath,
        const std::vector<std::string>& textureNames,
        const TextureParams& params
    )
    {
        if (params.desiredChannels < 1 || params.desiredChannels > 4)
        {
            std::cerr << "[TextureLoader][createAndLoadTextureArray]: Only 1-4 channels are supported. Got: " << params.desiredChannels<< "\n";
            return;
        }

        if (!fs::exists(texturesFolderPath))
        {
            std::cerr << "[TextureLoader][createAndLoadTextureArray]: Textures folder is not found for path " << texturesFolderPath<< "\n";
            return;
        }

        stbi_set_flip_vertically_on_load(true);

        // Read images' size and store maximum
        const size_t layerCount = textureNames.size();

        int sharedWidth = 0;
        int sharedHeight = 0;

        for (size_t i = 0; i < layerCount; i++)
        {
            fs::path fullPath = texturesFolderPath / (textureNames[i] + ".png");

            int width, height, channels;
            if (!stbi_info(fullPath.string().c_str(), &width, &height, &channels))
            {
                continue;
            }

            sharedWidth = std::max(sharedWidth, width);
            sharedHeight = std::max(sharedHeight, height);
        }

        //
        const int mipmapLevels = 1 + (params.createMipmaps ? static_cast<int>(std::ceil(std::log2(std::max(sharedWidth, sharedHeight)))) : 0);

        std::vector<unsigned char> undefinedTexture = createUndefinedTexture(sharedWidth, sharedHeight, params.desiredChannels);

        GLenum internalFormat = getInternalFormat(params.desiredChannels);
        GLenum format = getFormat(params.desiredChannels);

        // Create the texture array using OpenGL_Texture
        texture.create2DArray(sharedWidth, sharedHeight, static_cast<int>(layerCount), internalFormat, format, GL_UNSIGNED_BYTE, mipmapLevels);
        texture.bind();

        // Set texture parameters
        texture.setParameters(
            params.minFilter,
            params.magFilter,
            params.wrapMode,
            params.wrapMode,
            params.wrapMode);

        // Load textures
        std::vector<unsigned char> convertedData;
        for (size_t i = 0; i < layerCount; i++)
        {
            fs::path fullPath = texturesFolderPath / (textureNames[i] + ".png");

            int width, height, channels;
            unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &channels, 0);

            if (!data)
            {
                std::cerr << "[TextureLoader][createAndLoadTextureArray]: Failed to load texture: " << fullPath << "\n";
                // Upload fallback texture
                texture.uploadSubData(undefinedTexture.data(), 0, 0, static_cast<int>(i), sharedWidth, sharedHeight, 1, 0);
                continue;
            }

            // Convert data to desired channel format
            if (channels != params.desiredChannels)
            {
                const int minChannels = std::min(params.desiredChannels, channels);
                convertedData.resize(sharedWidth * sharedHeight * params.desiredChannels);
                const int srcStride = sharedWidth * channels;
                const int dstStride = sharedWidth * params.desiredChannels;

                for (int y = 0; y < sharedHeight; y++)
                {
                    for (int x = 0; x < sharedWidth; x++)
                    {
                        int srcIndex = y * srcStride + x * channels;
                        int dstIndex = y * dstStride + x * params.desiredChannels;

                        for (int c = 0; c < minChannels; c++)
                        {
                            convertedData[dstIndex + c] = data[srcIndex + c];
                        }
                    }
                }
            }

            // Upload the texture data
            const auto* readData = (channels == params.desiredChannels) ? data : convertedData.data();
            texture.uploadSubData(readData, 0, 0, static_cast<int>(i), sharedWidth, sharedHeight, 1, 0);

            stbi_image_free(data);
        }

        // Generate mipmaps
        if (params.createMipmaps)
        {
            texture.generateMipmaps();
        }
    }

    void createAndLoadTexture3DFromFloatData(OpenGL_Texture& texture, const std::vector<float>& data, int width, int height, int depth, const TextureParams& params)
    {
        if (params.desiredChannels < 1 || params.desiredChannels > 4)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture3DFromFloatData]: Only 1-4 channels are supported. Got: " << params.desiredChannels<< "\n";
            return;
        }

        const int expectedSize = width * height * depth;
        if (data.size() < expectedSize)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture3D]: Provided data size is not enough\n";
            return;
        }
        if (data.size() > expectedSize)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture3D]: Provided data size is too big, truncating\n";
        }

        const int mipmapLevels = 1 + (params.createMipmaps ? static_cast<int>(std::ceil(std::log2(
            std::max({ width, height, depth })
        ))) : 0);

        GLenum internalFormat = getInternalFormat(params.desiredChannels);
        GLenum format = getFormat(params.desiredChannels);

        texture.create3D(width, height, depth, internalFormat, format, GL_FLOAT, mipmapLevels);
        texture.bind();

        texture.setParameters(
            params.minFilter,
            params.magFilter,
            params.wrapMode,
            params.wrapMode,
            params.wrapMode);

        // Upload the texture data
        texture.uploadSubData(data.data(), 0, 0, 0, width, height, depth, 0);
    }
}