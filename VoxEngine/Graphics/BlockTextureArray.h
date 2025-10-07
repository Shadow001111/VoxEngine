#pragma once
#include <glad/glad.h>

#include <string>
#include <vector>

class BlockTextureArray
{
	GLuint ID;
	GLuint unit;
public:
	BlockTextureArray(const std::string& texturesFolderPath, const std::vector<std::string>& textureNames, GLuint slot, int textureSize);
	~BlockTextureArray();

	void bind() const;
	void unbind() const;

	GLuint getUnit() const;
};

