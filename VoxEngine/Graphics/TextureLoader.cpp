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
        else if (channels == 3)
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
        else if (channels == 2)
        {
            size_t index = 0;
            for (int y = 0; y < textureSize; y++)
            {
                bool hy = y > (textureSize >> 1);
                for (int x = 0; x < textureSize; x++)
                {
                    bool hx = x > (textureSize >> 1);
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
            for (int y = 0; y < textureSize; y++)
            {
                bool hy = y > (textureSize >> 1);
                for (int x = 0; x < textureSize; x++)
                {
                    bool hx = x > (textureSize >> 1);
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

    void createAndLoadTexture2D(OpenGL_Texture& texture, const std::filesystem::path& texturePath, int textureSize, const TextureParams& params)
    {
        if (params.desiredChannels < 1 || params.desiredChannels > 4)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture2D]: Only 1-4 channels are supported. Got: " << params.desiredChannels << std::endl;
            return;
        }

        if (!fs::exists(texturePath) || !fs::is_regular_file(texturePath))
        {
            std::cerr << "[TextureLoader][createAndLoadTexture2D]: Texture file is not found " << texturePath << std::endl;
            return;
        }

        const int mipmapLevels = 1 + (params.createMipmaps ? static_cast<int>(std::ceil(std::log2(textureSize))) : 0);

        std::vector<unsigned char> undefinedTexture = createUndefinedTexture(textureSize, params.desiredChannels);

        GLenum internalFormat = getInternalFormat(params.desiredChannels);
        GLenum format = getFormat(params.desiredChannels);

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
        // Load with original channels first to see what we have
        unsigned char* data = stbi_load(texturePath.string().c_str(), &width, &height, &channels, 0);

        if (!data)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture2D]: Failed to load texture: " << texturePath << "\n";
            // Upload fallback texture
            texture.uploadSubData(undefinedTexture.data(), 0, 0, 0,
                textureSize, textureSize, 1, 0);
            return;
        }

        if (width != textureSize || height != textureSize)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture2D]: Texture " << texturePath.stem()
                << " is not " << textureSize << "x" << textureSize
                << " (" << width << "x" << height << ")\n";
            stbi_image_free(data);
            // Upload fallback texture
            texture.uploadSubData(undefinedTexture.data(), 0, 0, 0,
                textureSize, textureSize, 1, 0);
            return;
        }

        // Convert data to desired channel format
        std::vector<unsigned char> convertedData;
        if (channels != params.desiredChannels)
        {
            convertedData.resize(textureSize * textureSize * params.desiredChannels);
            int srcStride = textureSize * channels;
            int dstStride = textureSize * params.desiredChannels;

            for (int y = 0; y < textureSize; y++)
            {
                for (int x = 0; x < textureSize; x++)
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
            texture.uploadSubData(data, 0, 0, 0, textureSize, textureSize, 1, 0);
        }
        else
        {
            texture.uploadSubData(convertedData.data(), 0, 0, 0, textureSize, textureSize, 1, 0);
        }

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
        if (params.desiredChannels < 1 || params.desiredChannels > 4)
        {
            std::cerr << "[TextureLoader][createAndLoadTextureArray]: Only 1-4 channels are supported. Got: " << params.desiredChannels << std::endl;
            return;
        }

        if (!fs::exists(texturesFolderPath))
        {
            std::cerr << "[TextureLoader][createAndLoadTextureArray]: Textures folder is not found for path " << texturesFolderPath << std::endl;
            return;
        }

        const size_t layerCount = textureNames.size();
        const int mipmapLevels = 1 + (params.createMipmaps ? static_cast<int>(std::ceil(std::log2(textureSize))) : 0);

        std::vector<unsigned char> undefinedTexture = createUndefinedTexture(textureSize, params.desiredChannels);

        GLenum internalFormat = getInternalFormat(params.desiredChannels);
        GLenum format = getFormat(params.desiredChannels);

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
            // Load with original channels first to see what we have
            unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &channels, 0);

            if (!data)
            {
                std::cerr << "[TextureLoader][createAndLoadTextureArray]: Failed to load texture: " << fullPath << "\n";
                // Upload fallback texture
                texture.uploadSubData(undefinedTexture.data(), 0, 0, static_cast<int>(i),
                    textureSize, textureSize, 1, 0);
                continue;
            }

            if (width != textureSize || height != textureSize)
            {
                std::cerr << "[TextureLoader][createAndLoadTextureArray]: Texture " << textureNames[i]
                    << " is not " << textureSize << "x" << textureSize
                    << " (" << width << "x" << height << ")\n";
                stbi_image_free(data);
                // Upload fallback texture
                texture.uploadSubData(undefinedTexture.data(), 0, 0, static_cast<int>(i),
                    textureSize, textureSize, 1, 0);
                continue;
            }

            // Convert data to desired channel format
            std::vector<unsigned char> convertedData;
            if (channels != params.desiredChannels)
            {
                convertedData.resize(textureSize * textureSize * params.desiredChannels);
                int srcStride = textureSize * channels;
                int dstStride = textureSize * params.desiredChannels;

                for (int y = 0; y < textureSize; y++)
                {
                    for (int x = 0; x < textureSize; x++)
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
                texture.uploadSubData(data, 0, 0, static_cast<int>(i),
                    textureSize, textureSize, 1, 0);
            }
            else
            {
                texture.uploadSubData(convertedData.data(), 0, 0, static_cast<int>(i),
                    textureSize, textureSize, 1, 0);
            }

            stbi_image_free(data);
        }

        // Generate mipmaps
        if (params.createMipmaps)
        {
            texture.generateMipmaps();
        }
    }

    void createAndLoadTexture3DFromFloatData(OpenGL_Texture& texture, const std::vector<float>& data, int textureSize, const TextureParams& params)
    {
        if (params.desiredChannels < 1 || params.desiredChannels > 4)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture3DFromFloatData]: Only 1-4 channels are supported. Got: " << params.desiredChannels << std::endl;
            return;
        }

        const int expectedSize = textureSize * textureSize * textureSize;
        if (data.size() < expectedSize)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture3D]: Provided data size is not enough\n";
            return;
        }
        if (data.size() > expectedSize)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture3D]: Provided data size is too big, truncating\n";
        }

        const int mipmapLevels = 1 + (params.createMipmaps ? static_cast<int>(std::ceil(std::log2(textureSize))) : 0);

        GLenum internalFormat = getInternalFormat(params.desiredChannels);
        GLenum format = getFormat(params.desiredChannels);

        texture.create3D(textureSize, textureSize, textureSize, internalFormat, format, GL_FLOAT, mipmapLevels);
        texture.bind();

        texture.setParameters(
            params.minFilter,
            params.magFilter,
            params.wrapMode,
            params.wrapMode,
            params.wrapMode);

        // Upload the texture data
        texture.uploadSubData(data.data(), 0, 0, 0, textureSize, textureSize, textureSize, 0);
    }
}