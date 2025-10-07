#include "BlockTextureArray.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <iostream>

BlockTextureArray::BlockTextureArray(const std::string& texturesFolderPath, const std::vector<std::string>& textureNames, GLuint slot, int textureSize) :
	ID(0), unit(slot)
{
    const bool createMipmaps = true;
    const int mipmapLevels = 1 + (createMipmaps ? ceilf(log2f(textureSize)) : 0);

    // Generate texture
	glGenTextures(1, &ID);
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D_ARRAY, ID);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipmapLevels, GL_RGB8, textureSize, textureSize, static_cast<GLsizei>(textureNames.size()));

    // Load
	stbi_set_flip_vertically_on_load(true);

    const int desiredChannels = 3;

    for (size_t i = 0; i < textureNames.size(); ++i)
    {
        std::string fullPath = texturesFolderPath + "/" + textureNames[i] + ".png";
        int width, height, channels;
        unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &channels, desiredChannels);

        if (!data)
        {
            std::cerr << "Failed to load texture: " << fullPath << std::endl;
            glDeleteTextures(1, &ID);
            throw std::runtime_error("Texture loading failed");
        }

        if (width != textureSize || height != textureSize)
        {
            std::cerr << "Warning: Texture " << textureNames[i]
                << " is not " << textureSize << "x" << textureSize << "(" << width << "x" << height << ")" << std::endl;
        }

        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            0, 0, static_cast<GLint>(i),
            textureSize, textureSize, 1,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            data
        );

        stbi_image_free(data);
    }

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Generate mipmaps
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    // Unbind
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

BlockTextureArray::~BlockTextureArray()
{
    glDeleteTextures(1, &ID);
}

void BlockTextureArray::bind() const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
}

void BlockTextureArray::unbind() const
{
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

GLuint BlockTextureArray::getUnit() const
{
    return unit;
}
