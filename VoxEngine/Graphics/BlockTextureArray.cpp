#include "BlockTextureArray.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <iostream>
#include <cmath>

void BlockTextureArray::load(const std::string& texturesFolderPath, const std::vector<std::string>& textureNames, int textureSize)
{
    this->textureSize = textureSize;
    this->layerCount = textureNames.size();

    const bool createMipmaps = true;
    const int mipmapLevels = 1 + (createMipmaps ? static_cast<int>(std::ceil(std::log2(textureSize))) : 0);
    const int desiredChannels = 4;

    // Create fallback "undefined" texture (checkerboard pattern)
    std::vector<unsigned char> undefinedTexture(textureSize * textureSize * desiredChannels);

    if (desiredChannels == 4)
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
    else if (desiredChannels == 3)
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
    else
    {
        std::cerr << "[BlockTextureArray]: Textures with " << desiredChannels << " channels are not supported." << std::endl;
        return;
    }

    GLenum internalFormat = (desiredChannels == 4) ? GL_RGBA8 : GL_RGB8;
    GLenum format = (desiredChannels == 4) ? GL_RGBA : GL_RGB;

    // Create the texture array using OpenGL_Texture
    texture.create2DArray(textureSize, textureSize, static_cast<int>(textureNames.size()),
        internalFormat, format, GL_UNSIGNED_BYTE, mipmapLevels);
    texture.bind();

    // Set texture parameters
    texture.setParameters(GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    // Load textures
    stbi_set_flip_vertically_on_load(true);

    for (size_t i = 0; i < textureNames.size(); ++i)
    {
        std::string fullPath = texturesFolderPath + "/" + textureNames[i] + ".png";
        int width, height, channels;
        unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &channels, desiredChannels);

        if (!data)
        {
            std::cerr << "Failed to load texture: " << fullPath << std::endl;
            // Upload fallback texture
            texture.uploadSubData(undefinedTexture.data(), 0, 0, static_cast<int>(i),
                textureSize, textureSize, 1, 0);
            continue;
        }

        if (width != textureSize || height != textureSize)
        {
            std::cerr << "Warning: Texture " << textureNames[i]
                << " is not " << textureSize << "x" << textureSize
                << " (" << width << "x" << height << ")" << std::endl;
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
    texture.generateMipmaps();
}