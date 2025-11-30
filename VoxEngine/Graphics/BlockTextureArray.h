#pragma once
#include <glad/glad.h>

#include <string>
#include <vector>

class BlockTextureArray
{
	GLuint ID = 0;
public:
	BlockTextureArray();
	~BlockTextureArray();

	BlockTextureArray(const BlockTextureArray&) = delete;
	BlockTextureArray& operator=(const BlockTextureArray&) = delete;

	BlockTextureArray(BlockTextureArray&& other) noexcept;
	BlockTextureArray& operator=(BlockTextureArray&& other) noexcept;

	void load(const std::string& texturesFolderPath, const std::vector<std::string>& textureNames, int textureSize);

	void bind() const;
	void unbind() const;
};

