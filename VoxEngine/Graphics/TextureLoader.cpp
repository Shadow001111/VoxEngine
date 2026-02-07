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

    int calculateMipmapLevels(int width, int height, bool createMipmaps)
    {
        if (!createMipmaps) return 1;

        int maxSize = std::max(width, height);
        int levels = 1 + static_cast<int>(std::log2(maxSize));

        return levels;
    }

    int calculateMipmapLevels(int width, int height, int depth, bool createMipmaps)
    {
        if (!createMipmaps) return 1;

        int maxSize = std::max({ width, height, depth });
        int levels = 1 + static_cast<int>(std::log2(maxSize));

        return levels;
    }

    bool validateChannels(int channels)
    {
        if (channels < 1 || channels > 4)
        {
            std::cerr << "[TextureLoader]: Only 1-4 channels are supported. Got: " << channels << "\n";
            return false;
        }
        return true;
    }

    void convertImageData(
        const unsigned char* srcData,
        int width, int height,
        int srcChannels, int dstChannels,
        std::vector<unsigned char>& dstData)
    {
        dstData.resize(width * height * dstChannels);
        const int srcStride = width * srcChannels;
        const int dstStride = width * dstChannels;

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                int srcIndex = y * srcStride + x * srcChannels;
                int dstIndex = y * dstStride + x * dstChannels;

                // Copy available channels
                int minChannels = std::min(srcChannels, dstChannels);
                for (int c = 0; c < minChannels; c++)
                {
                    dstData[dstIndex + c] = srcData[srcIndex + c];
                }

                // Pad missing channels
                for (int c = minChannels; c < dstChannels; c++)
                {
                    if (dstChannels == 4 && c == 3)
                    {
                        // Alpha channel - default to fully opaque
                        dstData[dstIndex + c] = 255;
                    }
                    else if (dstChannels == 2 && c == 1)
                    {
                        // Second channel in 2-channel texture
                        dstData[dstIndex + c] = 255;
                    }
                    else
                    {
                        dstData[dstIndex + c] = 0;
                    }
                }
            }
        }
    }

    void loadAndProcessImage(
        const std::filesystem::path& path,
        int desiredChannels,
        std::vector<unsigned char>& outputData,
        int& width, int& height)
    {
        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            std::cerr << "[TextureLoader]: Texture file not found: " << path << "\n";
            width = height = 0;
            return;
        }

        stbi_set_flip_vertically_on_load(true);

        int channels;
        unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 0);

        if (!data)
        {
            std::cerr << "[TextureLoader]: Failed to load texture: " << path << "\n";
            width = height = 0;
            return;
        }

        if (channels != desiredChannels)
        {
            convertImageData(data, width, height, channels, desiredChannels, outputData);
        }
        else
        {
            outputData.assign(data, data + width * height * channels);
        }

        stbi_image_free(data);
    }



    void createTexture2DFromImage(
        Texture& texture,
        const std::filesystem::path& texturePath,
        const TextureLoadParams& params)
    {
        if (!validateChannels(params.desiredChannels)) return;

        std::vector<unsigned char> imageData;
        int width, height;

        loadAndProcessImage(texturePath, params.desiredChannels, imageData, width, height);

        if (width == 0 || height == 0)
        {
            // Create fallback texture
            constexpr int defaultSize = 16;
            std::vector<unsigned char> undefinedTexture = createUndefinedTexture(defaultSize, defaultSize, params.desiredChannels);

            const int mipmapLevels = calculateMipmapLevels(defaultSize, defaultSize, params.createMipmaps);
            GLenum internalFormat = getInternalFormat(params.desiredChannels);

            texture.create2D(defaultSize, defaultSize, internalFormat, mipmapLevels);
            texture.uploadSubData2D(
                undefinedTexture.data(),
                0, 0,
                defaultSize, defaultSize,
                GL_UNSIGNED_BYTE,
                0);
            if (mipmapLevels > 1) texture.generateMipmaps();
            return;
        }

        const int mipmapLevels = calculateMipmapLevels(width, height, params.createMipmaps);
        GLenum internalFormat = getInternalFormat(params.desiredChannels);

        texture.create2D(width, height, internalFormat, mipmapLevels);
        texture.uploadSubData2D(
            imageData.data(),
            0, 0,
            width, height,
            GL_UNSIGNED_BYTE,
            0);
        if (mipmapLevels > 1) texture.generateMipmaps();
    }

    void createTextureArrayFromImages(
        Texture& texture,
        const fs::path& texturesFolderPath,
        const std::vector<std::string>& textureNames,
        const TextureLoadParams& params
    )
    {
        if (!validateChannels(params.desiredChannels)) return;

        if (!fs::exists(texturesFolderPath))
        {
            std::cerr << "[TextureLoader]: Textures folder not found: " << texturesFolderPath << "\n";
            return;
        }

        stbi_set_flip_vertically_on_load(true);

        const size_t layerCount = textureNames.size();

        // Determine maximum dimensions
        int sharedWidth = 0;
        int sharedHeight = 0;
        // TODO: Add 'sharedChannels'

        for (size_t i = 0; i < layerCount; i++)
        {
            fs::path fullPath = texturesFolderPath / (textureNames[i] + ".png");

            int width, height, channels;
            if (stbi_info(fullPath.string().c_str(), &width, &height, &channels))
            {
                sharedWidth = std::max(sharedWidth, width);
                sharedHeight = std::max(sharedHeight, height);
            }
        }

        if (sharedWidth == 0 || sharedHeight == 0)
        {
            sharedWidth = sharedHeight = 16; // Default fallback size
        }

        const int mipmapLevels = calculateMipmapLevels(sharedWidth, sharedHeight, params.createMipmaps);
        GLenum internalFormat = getInternalFormat(params.desiredChannels);
        std::vector<unsigned char> undefinedTexture = createUndefinedTexture(sharedWidth, sharedHeight, params.desiredChannels);

        texture.create2DArray(sharedWidth, sharedHeight, static_cast<int>(layerCount), internalFormat, mipmapLevels);

        // Load and upload each texture
        std::vector<unsigned char> imageData;
        std::vector<unsigned char> convertedData;

        for (size_t i = 0; i < layerCount; i++)
        {
            fs::path fullPath = texturesFolderPath / (textureNames[i] + ".png");

            int width, height;
            loadAndProcessImage(fullPath, params.desiredChannels, imageData, width, height);

            const unsigned char* uploadData = nullptr;
            int uploadWidth = sharedWidth;
            int uploadHeight = sharedHeight;

            if (width == 0 || height == 0)
            {
                // Use undefined texture as fallback
                uploadData = undefinedTexture.data();
            }
            else if (width != sharedWidth || height != sharedHeight)
            {
                // Resize or pad image data to match shared dimensions
                convertImageData(imageData.data(), width, height, params.desiredChannels, params.desiredChannels, convertedData);
                uploadData = convertedData.data();
            }
            else
            {
                uploadData = imageData.data();
            }

            texture.uploadSubData2DArray(
                uploadData,
                0, 0, static_cast<int>(i),
                uploadWidth, uploadHeight,
                GL_UNSIGNED_BYTE,
                0);
        }

        if (mipmapLevels > 1) texture.generateMipmaps();
    }

    void createTexture3DFromFloatData(Texture& texture, const std::vector<float>& data, int width, int height, int depth, const TextureLoadParams& params)
    {
        if (!validateChannels(params.desiredChannels)) return;

        const int expectedSize = width * height * depth;
        if (data.size() < expectedSize)
        {
            std::cerr << "[TextureLoader][createAndLoadTexture3D]: Provided data size is not enough\n";
            return;
        }

        const int mipmapLevels = 1 + calculateMipmapLevels(width, height, depth, params.createMipmaps);

        GLenum internalFormat = getInternalFormat(params.desiredChannels);
        GLenum format = getFormat(params.desiredChannels);

        texture.create3D(width, height, depth, internalFormat, mipmapLevels);

        // Upload the texture data
        texture.uploadSubData3D(
            data.data(),
            0, 0, 0,
            width, height, depth,
            GL_FLOAT,
            0);
        if (mipmapLevels > 1) texture.generateMipmaps();
    }
}