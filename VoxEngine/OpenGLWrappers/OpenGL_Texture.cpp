#include "OpenGL_Texture.h"
#include <iostream>


//namespace TextureCompression
//{
//	CompressionSupport g_CompressionSupport;
//
//	void setCompressionFormats()
//	{
//		g_CompressionSupport.s3tc = GLAD_GL_EXT_texture_compression_s3tc;
//		g_CompressionSupport.rgtc = GLAD_GL_ARB_texture_compression_rgtc;
//		g_CompressionSupport.bptc = GLAD_GL_ARB_texture_compression_bptc;
//		g_CompressionSupport.astc = GLAD_GL_KHR_texture_compression_astc_ldr;
//	}
//
//	GLenum getBestFormat(int channels, GLenum valueType)
//	{
//		return GLenum();
//	}
//
//	GLenum getBestCompressedFormat(int channels, GLenum valueType)
//	{
//		return GLenum();
//	}
//}

OpenGL_Texture::~OpenGL_Texture()
{
	if (id) glDeleteTextures(1, &id);
}

OpenGL_Texture::OpenGL_Texture(OpenGL_Texture&& other) noexcept :
	id(other.id), type(other.type),
	internalFormat(other.internalFormat), format(other.format),dataType(other.dataType),
	width(other.width), height(other.height), depth(other.depth), mipLevels(other.mipLevels),
	minFilter(other.minFilter), magFilter(other.magFilter),
	wrapS(other.wrapS), wrapT(other.wrapT), wrapR(other.wrapR)

{
	other.id = 0;
	other.width = 0;
	other.height = 0;
	other.depth = 0;
	other.mipLevels = 1;
}

OpenGL_Texture& OpenGL_Texture::operator=(OpenGL_Texture&& other) noexcept
{
	if (this != &other)
	{
		if (id) glDeleteTextures(1, &id);

		id = other.id;
		type = other.type;
		internalFormat = other.internalFormat;
		format = other.format;
		dataType = other.dataType;
		width = other.width;
		height = other.height;
		depth = other.depth;
		mipLevels = other.mipLevels;
		wrapS = other.wrapS;
		wrapT = other.wrapT;
		wrapR = other.wrapR;

		other.id = 0;
		other.width = 0;
		other.height = 0;
		other.depth = 0;
		other.mipLevels = 1;
	}
	return *this;
}

void OpenGL_Texture::create1D(int width, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels)
{
	if (width <= 0)
	{
		std::cerr << "[OpenGL_Texture][create1D]: Invalid texture dimensions: width=" << width << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[OpenGL_Texture][create1D]: Invalid mipLevels: " << mipLevels << "\n";
		return;
	}

	this->type = GL_TEXTURE_1D;
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
	this->width = width;
	this->height = 1;
	this->depth = 1;
	this->mipLevels = mipLevels;

	if (id == 0)
	{
		glCreateTextures(this->type, 1, &id);
	}
	glTextureStorage1D(id, mipLevels, internalFormat, width);
}

void OpenGL_Texture::create2D(int width, int height, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels)
{
	if (width <= 0 || height <= 0)
	{
		std::cerr << "[OpenGL_Texture][create2D]: Invalid texture dimensions: width=" << width << ", height=" << height << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[OpenGL_Texture][create2D]: Invalid mipLevels: " << mipLevels << "\n";
		return;
	}

	this->type = GL_TEXTURE_2D;
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
	this->width = width;
	this->height = height;
	this->depth = 1;
	this->mipLevels = mipLevels;

	if (id == 0)
	{
		glCreateTextures(this->type, 1, &id);
	}
	glTextureStorage2D(id, mipLevels, internalFormat, width, height);
}

void OpenGL_Texture::create3D(int width, int height, int depth, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels)
{
	if (width <= 0 || height <= 0 || depth <= 0)
	{
		std::cerr << "[OpenGL_Texture][create3D]: Invalid texture dimensions: width=" << width << ", height=" << height << ", depth=" << depth << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[OpenGL_Texture][create3D]: Invalid mipLevels: " << mipLevels << "\n";
		return;
	}

	this->type = GL_TEXTURE_3D;
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
	this->width = width;
	this->height = height;
	this->depth = depth;
	this->mipLevels = mipLevels;

	if (id == 0)
	{
		glCreateTextures(this->type, 1, &id);
	}
	glTextureStorage3D(id, mipLevels, internalFormat, width, height, depth);
}

void OpenGL_Texture::create2DArray(int width, int height, int layers, GLenum internalFormat, GLenum format, GLenum dataType, int mipLevels)
{
	if (width <= 0 || height <= 0 || layers <= 0)
	{
		std::cerr << "[OpenGL_Texture][create2DArray]: Invalid texture array dimensions: width=" << width << ", height=" << height << ", layers=" << layers << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[OpenGL_Texture][create2DArray]: Invalid mipLevels: " << mipLevels << "\n";
		return;
	}

	this->type = GL_TEXTURE_2D_ARRAY;
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
	this->width = width;
	this->height = height;
	this->depth = layers;
	this->mipLevels = mipLevels;

	if (id == 0)
	{
		glCreateTextures(this->type, 1, &id);
	}
	glTextureStorage3D(id, mipLevels, internalFormat, width, height, layers);
}

void OpenGL_Texture::recreate1D(int width)
{
	if (width <= 0)
	{
		std::cerr << "[OpenGL_Texture][recreate1D]: Invalid texture dimensions: width=" << width << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[OpenGL_Texture][recreate1D]: Invalid mipLevels: " << mipLevels << "\n";
		return;
	}

	this->width = width;

	if (id)
	{
		glDeleteTextures(1, &id);
	}
	glCreateTextures(this->type, 1, &id);

	glTextureStorage1D(id, mipLevels, internalFormat, width);
	
	glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilter);
	glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilter);
	glTextureParameteri(id, GL_TEXTURE_WRAP_S, wrapS);
}

void OpenGL_Texture::recreate2D(int width, int height)
{
	if (width <= 0 || height <= 0)
	{
		std::cerr << "[OpenGL_Texture][recreate2D]: Invalid texture dimensions: width=" << width << ", height=" << height << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[OpenGL_Texture][recreate2D]: Invalid mipLevels: " << mipLevels << "\n";
		return;
	}

	this->width = width;
	this->height = height;

	if (id)
	{
		glDeleteTextures(1, &id);
	}
	glCreateTextures(this->type, 1, &id);

	glTextureStorage2D(id, mipLevels, internalFormat, width, height);

	glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilter);
	glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilter);
	glTextureParameteri(id, GL_TEXTURE_WRAP_S, wrapS);
	glTextureParameteri(id, GL_TEXTURE_WRAP_T, wrapT);
}

void OpenGL_Texture::recreate3D(int width, int height, int depth)
{
	if (width <= 0 || height <= 0 || depth <= 0)
	{
		std::cerr << "[OpenGL_Texture][recreate3D]: Invalid texture dimensions: width=" << width << ", height=" << height << ", depth=" << depth << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[OpenGL_Texture][recreate3D]: Invalid mipLevels: " << mipLevels << "\n";
		return;
	}

	this->width = width;
	this->height = height;
	this->depth = depth;

	if (id)
	{
		glDeleteTextures(1, &id);
	}
	glCreateTextures(this->type, 1, &id);

	glTextureStorage3D(id, mipLevels, internalFormat, width, height, depth);

	glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilter);
	glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilter);
	glTextureParameteri(id, GL_TEXTURE_WRAP_S, wrapS);
	glTextureParameteri(id, GL_TEXTURE_WRAP_T, wrapT);
	glTextureParameteri(id, GL_TEXTURE_WRAP_R, wrapR);
}

void OpenGL_Texture::uploadData(const void* data, int level)
{
	switch (type)
	{
	case GL_TEXTURE_1D:
		glTextureSubImage1D(id, level, 0, width, format, dataType, data);
		break;
	case GL_TEXTURE_2D:
		glTextureSubImage2D(id, level, 0, 0, width, height, format, dataType, data);
		break;
	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
		glTextureSubImage3D(id, level, 0, 0, 0, width, height, depth, format, dataType, data);
		break;
	default:
		std::cerr << "[OpenGL_Texture][uploadData]: Unsupported texture type for data upload: " << type<< "\n";
		break;
	}
}

void OpenGL_Texture::uploadSubData(
	const void* data, int xOffset, int yOffset, int zOffset,
	int width, int height, int depth, int level)
{
	switch (type)
	{
	case GL_TEXTURE_1D:
		glTextureSubImage1D(id, level, xOffset, width, format, dataType, data);
		break;
	case GL_TEXTURE_2D:
		glTextureSubImage2D(id, level, xOffset, yOffset, width, height, format, dataType, data);
		break;
	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
		glTextureSubImage3D(id, level, xOffset, yOffset, zOffset, width, height, depth, format, dataType, data);
		break;
	default:
		std::cerr << "[OpenGL_Texture][uploadSubData]: Unsupported texture type for sub data upload: " << type<< "\n";
		break;
	}
}

void OpenGL_Texture::generateMipmaps()
{
	if (mipLevels > 1)
	{
		glGenerateTextureMipmap(id);
	}
}

void OpenGL_Texture::setParameters(GLenum minFilter_, GLenum magFilter_, GLenum wrapS_)
{
	minFilter = minFilter_;
	magFilter = magFilter_;
	wrapS = wrapS_;

	glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilter);
	glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilter);
	glTextureParameteri(id, GL_TEXTURE_WRAP_S, wrapS);
}

void OpenGL_Texture::setParameters(GLenum minFilter_, GLenum magFilter_, GLenum wrapS_, GLenum wrapT_)
{
	minFilter = minFilter_;
	magFilter = magFilter_;
	wrapS = wrapS_;
	wrapT = wrapT_;

	glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilter);
	glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilter);
	glTextureParameteri(id, GL_TEXTURE_WRAP_S, wrapS);
	glTextureParameteri(id, GL_TEXTURE_WRAP_T, wrapT);
}

void OpenGL_Texture::setParameters(GLenum minFilter_, GLenum magFilter_, GLenum wrapS_, GLenum wrapT_, GLenum wrapR_)
{
	minFilter = minFilter_;
	magFilter = magFilter_;
	wrapS = wrapS_;
	wrapT = wrapT_;
	wrapR = wrapR_;

	glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilter);
	glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilter);
	glTextureParameteri(id, GL_TEXTURE_WRAP_S, wrapS);
	glTextureParameteri(id, GL_TEXTURE_WRAP_T, wrapT);
	glTextureParameteri(id, GL_TEXTURE_WRAP_R, wrapR);
}

void OpenGL_Texture::bind() const
{
	glBindTexture(type, id);
}

void OpenGL_Texture::bind(GLenum target) const
{
	glBindTexture(target, id);
}

void OpenGL_Texture::unbind() const
{
	glBindTexture(type, 0);
}

void OpenGL_Texture::unbind(GLenum target)
{
	glBindTexture(target, 0);
}

void OpenGL_Texture::bindUnit(GLuint unit) const
{
	glBindTextureUnit(unit, id);
}
