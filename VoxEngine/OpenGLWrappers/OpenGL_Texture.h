#pragma once
#include <glad/glad.h>

class OpenGL_Texture
{
	GLuint id = 0;
	GLenum type = GL_TEXTURE_2D;
	GLenum internalFormat = 0;
	GLenum format = 0;
	GLenum dataType = 0;
	int width = 0;
	int height = 0;
	int depth = 0;
	int mipLevels = 1;
public:
	OpenGL_Texture();
	~OpenGL_Texture();

	OpenGL_Texture(const OpenGL_Texture&) = delete;
	OpenGL_Texture& operator=(const OpenGL_Texture&) = delete;

	OpenGL_Texture(OpenGL_Texture&& other) noexcept;
	OpenGL_Texture& operator=(OpenGL_Texture&& other) noexcept;

	// Texture creation functions for different types
	void create1D(int width, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels = 1);
	void create2D(int width, int height, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels = 1);
	void create3D(int width, int height, int depth, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels = 1);
	void create2DArray(int width, int height, int layers, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels = 1);
	void createCubeMap(int size, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels = 1);

	// Texture resizing functions
	void resize1D(int width);
	void resize2D(int width, int height);
	void resize3D(int width, int height, int depth);

	// Data upload functions
	void uploadData(const void* data, int level = 0);
	void uploadSubData(const void* data, int xOffset, int yOffset, int zOffset, int width, int height, int depth, int level = 0);

	//
	void generateMipmaps();

	void setParameters(GLenum minFilter, GLenum magFilter,
		GLenum wrapS, GLenum wrapT, GLenum wrapR = GL_CLAMP_TO_EDGE);

	void bind(GLuint unit) const;
	void bind() const;
	void unbind() const;

	// Getters
	GLuint getID() const { return id; }
	GLenum getType() const { return type; }
	GLenum getInternalFormat() const { return internalFormat; }
	GLenum getFormat() const { return format; }
	GLenum getDataType() const { return dataType; }
	int getWidth() const { return width; }
	int getHeight() const { return height; }
	int getDepth() const { return depth; }
	int getMipLevels() const { return mipLevels; }
};