#pragma once
#include "OpenGLWrappers/OpenGL_Texture.h"
#include <string>
#include <vector>

class BlockTextureArray
{
    OpenGL_Texture texture;
    int textureSize = 0;
    size_t layerCount = 0;
public:
    BlockTextureArray() = default;
    ~BlockTextureArray() = default;

    BlockTextureArray(const BlockTextureArray&) = delete;
    BlockTextureArray& operator=(const BlockTextureArray&) = delete;

    BlockTextureArray(BlockTextureArray&& other) noexcept = default;
    BlockTextureArray& operator=(BlockTextureArray&& other) noexcept = default;

    void load(const std::string& texturesFolderPath, const std::vector<std::string>& textureNames, int textureSize);

    void bind(GLuint unit = 0) const { texture.bind(unit); }
    void unbind() const { texture.unbind(); }

    // Getters
    GLuint getID() const { return texture.getID(); }
    int getTextureSize() const { return textureSize; }
    size_t getLayerCount() const { return layerCount; }
};