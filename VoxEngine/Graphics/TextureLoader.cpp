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

    // Convert int channel count to the TextureCompression::Channels enum.
    static TextureCompression::Channels toCompressionChannels(int channels)
    {
        switch (channels)
        {
        case 1:  return TextureCompression::Channels::R;
        case 2:  return TextureCompression::Channels::RG;
        case 3:  return TextureCompression::Channels::RGB;
        default: return TextureCompression::Channels::RGBA;
        }
    }

    // Returns the best internal format for the given params.
    // When compression is requested it resolves a compressed format and falls
    // back to the plain uncompressed format if the extension is unavailable.
    static GLenum resolveInternalFormat(const TextureLoadParams& params, bool allowCompression = true)
    {
        if (allowCompression && params.compression != TextureCompression::Format::NONE)
        {
            GLenum compressed = TextureCompression::resolveInternalFormat(
                params.compression,
                toCompressionChannels(params.desiredChannels),
                params.isHDR);

            if (compressed != GL_NONE)
                return compressed;

            std::cerr << "[TextureLoader]: Requested compression ("
                << TextureCompression::getName(params.compression)
                << ") is unavailable. Falling back to uncompressed storage.\n";
        }

        // Plain uncompressed fallback
        switch (params.desiredChannels)
        {
        case 1:  return GL_R8;
        case 2:  return GL_RG8;
        case 3:  return GL_RGB8;
        default: return GL_RGBA8;
        }
    }

    static int calculateMipmapLevels(int width, int height, bool createMipmaps)
    {
        if (!createMipmaps) return 1;

        int maxSize = std::max(width, height);
        int levels = 1 + static_cast<int>(std::log2(maxSize));

        return levels;
    }

    static int calculateMipmapLevels(int width, int height, int depth, bool createMipmaps)
    {
        if (!createMipmaps) return 1;

        int maxSize = std::max({ width, height, depth });
        int levels = 1 + static_cast<int>(std::log2(maxSize));

        return levels;
    }

    static bool validateChannels(int channels)
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

    // -----------------------------------------------------------------------
    //  Public API
    // -----------------------------------------------------------------------

    void createTexture2DFromImage(
        Texture& texture,
        const fs::path& texturePath,
        const TextureLoadParams& params)
    {
        if (!validateChannels(params.desiredChannels)) return;

        std::vector<unsigned char> imageData;
        int width = 0, height = 0;

        loadAndProcessImage(texturePath, params.desiredChannels, imageData, width, height);

        if (width == 0 || height == 0)
        {
            // Fallback: create a small magenta checkerboard.
            // Compression is intentionally skipped for the fallback texture
            // because the driver may produce poor quality on tiny images.
            constexpr int defaultSize = 16;
            std::vector<unsigned char> fallback =
                createUndefinedTexture(defaultSize, defaultSize, params.desiredChannels);

            const int mips = calculateMipmapLevels(defaultSize, defaultSize, params.createMipmaps);

            TextureLoadParams uncompressedParams = params;
            uncompressedParams.compression = TextureCompression::Format::NONE;
            GLenum fmt = resolveInternalFormat(uncompressedParams);

            texture.create2D(defaultSize, defaultSize, fmt, mips);
            texture.uploadSubData2D(fallback.data(), 0, 0, defaultSize, defaultSize, GL_UNSIGNED_BYTE, 0);
            if (mips > 1) texture.generateMipmaps();
            return;
        }

        const int  mips = calculateMipmapLevels(width, height, params.createMipmaps);
        const GLenum fmt = resolveInternalFormat(params);

        // When the internal format is compressed, OpenGL accepts ordinary
        // pixel data via glTextureSubImage* and compresses it on the driver.
        // No special upload path is required here.
        texture.create2D(width, height, fmt, mips);
        texture.uploadSubData2D(imageData.data(), 0, 0, width, height, GL_UNSIGNED_BYTE, 0);
        if (mips > 1) texture.generateMipmaps();
    }

    void createTextureArrayFromImages(
        Texture& texture,
        const fs::path& texturesFolderPath,
        const std::vector<std::string>& textureNames,
        const TextureLoadParams& params)
    {
        if (!validateChannels(params.desiredChannels)) return;

        if (!fs::exists(texturesFolderPath))
        {
            std::cerr << "[TextureLoader]: Textures folder not found: " << texturesFolderPath << "\n";
            return;
        }

        stbi_set_flip_vertically_on_load(true);

        const size_t layerCount = textureNames.size();

        // Determine shared dimensions across all layers.
        int sharedWidth = 0;
        int sharedHeight = 0;

        for (const auto& name : textureNames)
        {
            fs::path fullPath = texturesFolderPath / (name + ".png");
            int w, h, ch;
            if (stbi_info(fullPath.string().c_str(), &w, &h, &ch))
            {
                sharedWidth = std::max(sharedWidth, w);
                sharedHeight = std::max(sharedHeight, h);
            }
        }

        if (sharedWidth == 0 || sharedHeight == 0)
            sharedWidth = sharedHeight = 16;

        const int    mips = calculateMipmapLevels(sharedWidth, sharedHeight, params.createMipmaps);
        const GLenum fmt = resolveInternalFormat(params);

        std::vector<unsigned char> fallback =
            createUndefinedTexture(sharedWidth, sharedHeight, params.desiredChannels);

        texture.create2DArray(sharedWidth, sharedHeight, static_cast<int>(layerCount), fmt, mips);

        std::vector<unsigned char> imageData;
        std::vector<unsigned char> convertedData;

        for (size_t i = 0; i < layerCount; i++)
        {
            fs::path fullPath = texturesFolderPath / (textureNames[i] + ".png");

            int width = 0, height = 0;
            loadAndProcessImage(fullPath, params.desiredChannels, imageData, width, height);

            const unsigned char* uploadData = nullptr;

            if (width == 0 || height == 0)
            {
                uploadData = fallback.data();
            }
            else if (width != sharedWidth || height != sharedHeight)
            {
                convertImageData(
                    imageData.data(), width, height,
                    params.desiredChannels, params.desiredChannels,
                    convertedData);
                uploadData = convertedData.data();
            }
            else
            {
                uploadData = imageData.data();
            }

            texture.uploadSubData2DArray(
                uploadData,
                0, 0, static_cast<int>(i),
                sharedWidth, sharedHeight,
                GL_UNSIGNED_BYTE,
                0);
        }

        if (mips > 1) texture.generateMipmaps();
    }

    void createTexture3DFromFloatData(
        Texture& texture,
        const std::vector<float>& data,
        int width, int height, int depth,
        const TextureLoadParams& params)
    {
        if (!validateChannels(params.desiredChannels)) return;

        const int expectedSize = width * height * depth * params.desiredChannels;
        if (static_cast<int>(data.size()) < expectedSize)
        {
            std::cerr << "[TextureLoader][createTexture3DFromFloatData]: "
                "Provided data size (" << data.size() << ") is smaller than expected ("
                << expectedSize << ").\n";
            return;
        }

        // Block-compressed formats are not valid for GL_TEXTURE_3D in OpenGL.
        // Warn and fall back to uncompressed.
        if (params.compression != TextureCompression::Format::NONE)
        {
            std::cerr << "[TextureLoader][createTexture3DFromFloatData]: "
                "Block-compressed formats are not supported for 3D textures. "
                "Ignoring compression setting.\n";
        }

        TextureLoadParams uncompressedParams = params;
        uncompressedParams.compression = TextureCompression::Format::NONE;

        // Select a float-compatible internal format.
        GLenum internalFormat;
        switch (params.desiredChannels)
        {
        case 1:  internalFormat = GL_R32F;    break;
        case 2:  internalFormat = GL_RG32F;   break;
        case 3:  internalFormat = GL_RGB32F;  break;
        default: internalFormat = GL_RGBA32F; break;
        }

        const int mips = calculateMipmapLevels(width, height, depth, params.createMipmaps);

        texture.create3D(width, height, depth, internalFormat, mips);
        texture.uploadSubData3D(data.data(), 0, 0, 0, width, height, depth, GL_FLOAT, 0);
        if (mips > 1) texture.generateMipmaps();
    }
}