#include "OpenGL_Texture.h"
#include "OpenGLWrappers/openGLDebug.h"
#include <iostream>

OpenGL_Texture::OpenGL_Texture()
{
	glGenTextures(1, &id);
}

OpenGL_Texture::~OpenGL_Texture()
{
	if (id)
	{
		glDeleteTextures(1, &id);
	}
}

OpenGL_Texture::OpenGL_Texture(OpenGL_Texture&& other) noexcept
	: id(other.id), type(other.type), internalFormat(other.internalFormat),
	format(other.format), dataType(other.dataType), width(other.width),
	height(other.height), depth(other.depth), mipLevels(other.mipLevels)
{
	other.id = 0;
	other.type = GL_TEXTURE_2D;
	other.internalFormat = 0;
	other.format = 0;
	other.dataType = 0;
	other.width = 0;
	other.height = 0;
	other.depth = 0;
	other.mipLevels = 1;
}

OpenGL_Texture& OpenGL_Texture::operator=(OpenGL_Texture&& other) noexcept
{
	if (this != &other)
	{
		if (id)
		{
			glDeleteTextures(1, &id);
		}

		id = other.id;
		type = other.type;
		internalFormat = other.internalFormat;
		format = other.format;
		dataType = other.dataType;
		width = other.width;
		height = other.height;
		depth = other.depth;
		mipLevels = other.mipLevels;

		other.id = 0;
		other.type = GL_TEXTURE_2D;
		other.internalFormat = 0;
		other.format = 0;
		other.dataType = 0;
		other.width = 0;
		other.height = 0;
		other.depth = 0;
		other.mipLevels = 1;
	}
	return *this;
}

void OpenGL_Texture::create1D(int width, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels)
{
	this->type = GL_TEXTURE_1D;
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
	this->width = width;
	this->height = 1;
	this->depth = 1;
	this->mipLevels = mipLevels;

	glBindTexture(type, id);
	glTexStorage1D(type, mipLevels, internalFormat, width);
}

void OpenGL_Texture::create2D(int width, int height, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels)
{
	this->type = GL_TEXTURE_2D;
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
	this->width = width;
	this->height = height;
	this->depth = 1;
	this->mipLevels = mipLevels;

	glBindTexture(type, id);
	glTexStorage2D(type, mipLevels, internalFormat, width, height);
}

void OpenGL_Texture::create3D(int width, int height, int depth, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels)
{
	this->type = GL_TEXTURE_3D;
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
	this->width = width;
	this->height = height;
	this->depth = depth;
	this->mipLevels = mipLevels;

	glBindTexture(type, id);
	glTexStorage3D(type, mipLevels, internalFormat, width, height, depth);
}

void OpenGL_Texture::create2DArray(int width, int height, int layers, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels)
{
	this->type = GL_TEXTURE_2D_ARRAY;
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
	this->width = width;
	this->height = height;
	this->depth = layers;
	this->mipLevels = mipLevels;

	glBindTexture(type, id);
	glTexStorage3D(type, mipLevels, internalFormat, width, height, layers);
}

void OpenGL_Texture::createCubeMap(int size, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels)
{
	this->type = GL_TEXTURE_CUBE_MAP;
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
	this->width = size;
	this->height = size;
	this->depth = 6;
	this->mipLevels = mipLevels;

	glBindTexture(type, id);
	glTexStorage2D(type, mipLevels, internalFormat, size, size);
}

void OpenGL_Texture::resize1D(int width)
{
	this->width = width;

	glBindTexture(type, id);
	glTexStorage1D(type, mipLevels, internalFormat, width);
}

void OpenGL_Texture::resize2D(int width, int height)
{
	this->width = width;
	this->height = height;

	glBindTexture(type, id);
	glTexStorage2D(type, mipLevels, internalFormat, width, height);
}

void OpenGL_Texture::resize3D(int width, int height, int depth)
{
	this->width = width;
	this->height = height;
	this->depth = depth;

	glBindTexture(type, id);
	glTexStorage3D(type, mipLevels, internalFormat, width, height, depth);
}

void OpenGL_Texture::uploadData(const void* data, int level)
{
	OPENGL_CHECK_BIND_TARGET(id, type);

	switch (type)
	{
	case GL_TEXTURE_1D:
		glTexSubImage1D(type, level, 0, width, format, dataType, data);
		break;
	case GL_TEXTURE_2D:
		glTexSubImage2D(type, level, 0, 0, width, height, format, dataType, data);
		break;
	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
		glTexSubImage3D(type, level, 0, 0, 0, width, height, depth, format, dataType, data);
		break;
	case GL_TEXTURE_CUBE_MAP:
		std::cerr << "[OpenGL_Texture]: Use uploadSubData for cube map faces" << std::endl;
		break;
	default:
		std::cerr << "[OpenGL_Texture]: Unsupported texture type for data upload: " << type << std::endl;
		break;
	}
}

void OpenGL_Texture::uploadSubData(const void* data, int xOffset, int yOffset, int zOffset,
	int width, int height, int depth, int level)
{
	OPENGL_CHECK_BIND_TARGET(id, type);

	switch (type)
	{
	case GL_TEXTURE_1D:
		glTexSubImage1D(type, level, xOffset, width, format, dataType, data);
		break;
	case GL_TEXTURE_2D:
		glTexSubImage2D(type, level, xOffset, yOffset, width, height, format, dataType, data);
		break;
	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
		glTexSubImage3D(type, level, xOffset, yOffset, zOffset, width, height, depth, format, dataType, data);
		break;
	case GL_TEXTURE_CUBE_MAP:
		if (zOffset < 6)
		{
			glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + zOffset, level,
				xOffset, yOffset, width, height, format, dataType, data);
		}
		break;
	default:
		std::cerr << "[OpenGL_Texture]: Unsupported texture type for sub data upload: " << type << std::endl;
		break;
	}
}

void OpenGL_Texture::generateMipmaps()
{
	if (mipLevels > 1)
	{
		OPENGL_CHECK_BIND_TARGET(id, type);
		glGenerateMipmap(type);
	}
}

void OpenGL_Texture::setParameters(GLenum minFilter, GLenum magFilter,
	GLenum wrapS, GLenum wrapT, GLenum wrapR)
{
	OPENGL_CHECK_BIND_TARGET(id, type);

	glTexParameteri(type, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(type, GL_TEXTURE_MAG_FILTER, magFilter);
	glTexParameteri(type, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(type, GL_TEXTURE_WRAP_T, wrapT);

	if (type == GL_TEXTURE_3D || type == GL_TEXTURE_2D_ARRAY || type == GL_TEXTURE_CUBE_MAP)
	{
		glTexParameteri(type, GL_TEXTURE_WRAP_R, wrapR);
	}
}

void OpenGL_Texture::bind(GLuint unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(type, id);
}

void OpenGL_Texture::bind() const
{
	glBindTexture(type, id);
}

void OpenGL_Texture::unbind() const
{
	glBindTexture(type, 0);
}