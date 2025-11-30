#include "BlockTextureArray.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <iostream>

void addImageToTextureArray(unsigned char* data, int layer, int textureSize, GLenum format)
{
    glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,
        0, 0, static_cast<GLint>(layer),
        textureSize, textureSize, 1,
        format,
        GL_UNSIGNED_BYTE,
        data
    );
}

BlockTextureArray::BlockTextureArray()
{
    glGenTextures(1, &ID);
}

BlockTextureArray::~BlockTextureArray()
{
    if (ID)
    {
        glDeleteTextures(1, &ID);
    }
}

BlockTextureArray::BlockTextureArray(BlockTextureArray&& other) noexcept
    : ID(other.ID)
{
    other.ID = 0;
}

BlockTextureArray& BlockTextureArray::operator=(BlockTextureArray&& other) noexcept
{
    if (this != &other)
    {
        if (ID)
        {
            glDeleteTextures(1, &ID);
        }

        ID = other.ID;

        other.ID = 0;
    }
    return *this;
}

void BlockTextureArray::load(const std::string& texturesFolderPath, const std::vector<std::string>& textureNames, int textureSize)
{
    const bool createMipmaps = true;
    const int mipmapLevels = 1 + (createMipmaps ? ceilf(log2f(textureSize)) : 0);
    const int desiredChannels = 4;

    // Create fallback "undefined" texture (solid magenta)
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

                //
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

                //
                undefinedTexture[index] = r;
                undefinedTexture[index + 1] = g;
                undefinedTexture[index + 2] = b;
                index += 3;
            }
        }
    }
    else
    {
        std::cerr << "[BlockTextureArray]: Textures with " << desiredChannels << " are not supported." << std::endl;
    }

    GLenum internalFormat = (desiredChannels == 4) ? GL_RGBA8 : GL_RGB8;
    GLenum format = (desiredChannels == 4) ? GL_RGBA : GL_RGB;

    glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipmapLevels, internalFormat, textureSize, textureSize, static_cast<GLsizei>(textureNames.size()));

    // Load
    stbi_set_flip_vertically_on_load(true);

    for (size_t i = 0; i < textureNames.size(); ++i)
    {
        std::string fullPath = texturesFolderPath + "/" + textureNames[i] + ".png";
        int width, height, channels;
        unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &channels, desiredChannels);

        if (!data)
        {
            std::cerr << "Failed to load texture: " << fullPath << std::endl;
            addImageToTextureArray(undefinedTexture.data(), i, textureSize, format);
            continue;
        }

        if (width != textureSize || height != textureSize)
        {
            std::cerr << "Warning: Texture " << textureNames[i]
                << " is not " << textureSize << "x" << textureSize << " (" << width << "x" << height << ")" << std::endl;
            stbi_image_free(data);
            addImageToTextureArray(undefinedTexture.data(), i, textureSize, format);
            continue;
        }

        addImageToTextureArray(data, i, textureSize, format);

        stbi_image_free(data);
    }

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Generate mipmaps
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    // Unbind
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void BlockTextureArray::bind() const
{
    glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
}

void BlockTextureArray::unbind() const
{
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}
