#include "Texture.h"
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

#define TEXTURE_EXTENSION_WARNINGS 1

Texture::GlobalData Texture::globalData;


void Texture::applyParametrs() const
{
	glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, parametrs.minFilter);
	glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, parametrs.magFilter);
	glTextureParameteri(id, GL_TEXTURE_WRAP_S, parametrs.wrapS);
	glTextureParameteri(id, GL_TEXTURE_WRAP_T, parametrs.wrapT);
	glTextureParameteri(id, GL_TEXTURE_WRAP_R, parametrs.wrapR);
	glTextureParameteri(id, GL_TEXTURE_MAX_ANISOTROPY, parametrs.anisotropy);
}

Texture::~Texture()
{
	if (id) glDeleteTextures(1, &id);
}

Texture::Texture(Texture&& other) noexcept :
	id(other.id), type(other.type),
	internalFormat(other.internalFormat),
	width(other.width), height(other.height), depth(other.depth), mipLevels(other.mipLevels),
	parametrs(other.parametrs),
	handle(other.handle), resident(other.resident)

{
	other.id = 0;
	other.width = 0;
	other.height = 0;
	other.depth = 0;
	other.mipLevels = 1;
	other.handle = 0;
	other.resident = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
	if (this != &other)
	{
		if (id) glDeleteTextures(1, &id);

		id = other.id;
		type = other.type;
		internalFormat = other.internalFormat;
		width = other.width;
		height = other.height;
		depth = other.depth;
		mipLevels = other.mipLevels;
		parametrs = other.parametrs;
		handle = other.handle;
		resident = other.resident;

		other.id = 0;
		other.width = 0;
		other.height = 0;
		other.depth = 0;
		other.mipLevels = 1;
		other.handle = 0;
		other.resident = 0;
	}
	return *this;
}

void Texture::initGlobalData()
{
	// Extensions
	globalData.extensions.bindless = GLAD_GL_ARB_bindless_texture;

	// The rest
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &globalData.maxAnisotropy);
}

void Texture::create1D(texture_size width, GLenum internalFormat, mip_level mipLevels)
{
	if (width <= 0)
	{
		std::cerr << "[Texture][create1D]: Invalid texture dimensions: width=" << width << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[Texture][create1D]: Invalid mipLevels: " << (unsigned)mipLevels << "\n";
		return;
	}

	this->type = GL_TEXTURE_1D;
	this->internalFormat = internalFormat;
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

void Texture::create2D(texture_size width, texture_size height, GLenum internalFormat, mip_level mipLevels)
{
	if (width <= 0 || height <= 0)
	{
		std::cerr << "[Texture][create2D]: Invalid texture dimensions: width=" << width << ", height=" << height << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[Texture][create2D]: Invalid mipLevels: " << (unsigned)mipLevels << "\n";
		return;
	}

	this->type = GL_TEXTURE_2D;
	this->internalFormat = internalFormat;
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

void Texture::create3D(texture_size width, texture_size height, texture_size depth, GLenum internalFormat, mip_level mipLevels)
{
	if (width <= 0 || height <= 0 || depth <= 0)
	{
		std::cerr << "[Texture][create3D]: Invalid texture dimensions: width=" << width << ", height=" << height << ", depth=" << depth << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[Texture][create3D]: Invalid mipLevels: " << (unsigned)mipLevels << "\n";
		return;
	}

	this->type = GL_TEXTURE_3D;
	this->internalFormat = internalFormat;
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

void Texture::create2DArray(texture_size width, texture_size height, texture_size layers, GLenum internalFormat, mip_level mipLevels)
{
	if (width <= 0 || height <= 0 || layers <= 0)
	{
		std::cerr << "[Texture][create2DArray]: Invalid texture array dimensions: width=" << width << ", height=" << height << ", layers=" << layers << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[Texture][create2DArray]: Invalid mipLevels: " << (unsigned)mipLevels << "\n";
		return;
	}

	this->type = GL_TEXTURE_2D_ARRAY;
	this->internalFormat = internalFormat;
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

void Texture::recreate1D(texture_size width)
{
	if (width <= 0)
	{
		std::cerr << "[Texture][recreate1D]: Invalid texture dimensions: width=" << width << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[Texture][recreate1D]: Invalid mipLevels: " << (unsigned)mipLevels << "\n";
		return;
	}

	this->width = width;

	if (id)
	{
		glDeleteTextures(1, &id);
	}
	glCreateTextures(this->type, 1, &id);

	glTextureStorage1D(id, mipLevels, internalFormat, width);
	
	applyParametrs();

	if (id)
	{
		if (handle != 0) handle = glGetTextureHandleARB(id);
		if (resident) glMakeTextureHandleResidentARB(handle);
	}
}

void Texture::recreate2D(texture_size width, texture_size height)
{
	if (width <= 0 || height <= 0)
	{
		std::cerr << "[Texture][recreate2D]: Invalid texture dimensions: width=" << width << ", height=" << height << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[Texture][recreate2D]: Invalid mipLevels: " << (unsigned)mipLevels << "\n";
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

	applyParametrs();

	if (id)
	{
		if (handle != 0) handle = glGetTextureHandleARB(id);
		if (resident) glMakeTextureHandleResidentARB(handle);
	}
}

void Texture::recreate3D(texture_size width, texture_size height, texture_size depth)
{
	if (width <= 0 || height <= 0 || depth <= 0)
	{
		std::cerr << "[Texture][recreate3D]: Invalid texture dimensions: width=" << width << ", height=" << height << ", depth=" << depth << "\n";
		return;
	}
	if (mipLevels < 1)
	{
		std::cerr << "[Texture][recreate3D]: Invalid mipLevels: " << (unsigned)mipLevels << "\n";
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

	applyParametrs();

	if (id)
	{
		if (handle != 0) handle = glGetTextureHandleARB(id);
		if (resident) glMakeTextureHandleResidentARB(handle);
	}
}

void Texture::uploadData(
	const void* data, GLenum dataType, mip_level level)
{
	if (level < 0 || level >= mipLevels)
	{
		std::cerr << "[Texture][uploadData]: Invalid mipmap level: " << level << ". Texture has " << (unsigned)mipLevels << " mipLevels.\n";
		return;
	}

	auto format = getFormatFromInternalFormat();

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
		std::cerr << "[Texture][uploadData]: Unsupported texture type for data upload: " << type<< "\n";
		break;
	}
}

void Texture::uploadSubData1D(
	const void* data, texture_size xOffset,
	texture_size width, GLenum dataType, mip_level level)
{
	if (type != GL_TEXTURE_1D)
	{
		std::cerr << "[Texture][uploadSubData1D]: Texture is not 1D type. Actual type: " << type << "\n";
		return;
	}

	if (level < 0 || level >= mipLevels)
	{
		std::cerr << "[Texture][uploadSubData1D]: Invalid mipmap level: " << level
			<< ". Texture has " << (unsigned)mipLevels << " mipLevels.\n";
		return;
	}

	if (xOffset < 0 || width <= 0 || (xOffset + width) > this->width)
	{
		std::cerr << "[Texture][uploadSubData1D]: Invalid subregion: xOffset="
			<< xOffset << ", width=" << width << ", texture width=" << this->width << "\n";
		return;
	}

	auto format = getFormatFromInternalFormat();
	glTextureSubImage1D(id, level, xOffset, width, format, dataType, data);
}

void Texture::uploadSubData2D(
	const void* data, texture_size xOffset, texture_size yOffset,
	texture_size width, texture_size height, GLenum dataType, mip_level level)
{
	if (type != GL_TEXTURE_2D)
	{
		std::cerr << "[Texture][uploadSubData2D]: Texture is not 2D type. Actual type: " << type << "\n";
		return;
	}

	if (level < 0 || level >= mipLevels)
	{
		std::cerr << "[Texture][uploadSubData2D]: Invalid mipmap level: " << level
			<< ". Texture has " << (unsigned)mipLevels << " mipLevels.\n";
		return;
	}

	if (xOffset < 0 || yOffset < 0 || width <= 0 || height <= 0 ||
		(xOffset + width) > this->width || (yOffset + height) > this->height)
	{
		std::cerr << "[Texture][uploadSubData2D]: Invalid subregion: xOffset="
			<< xOffset << ", yOffset=" << yOffset << ", width=" << width
			<< ", height=" << height << ", texture size=" << this->width
			<< "x" << this->height << "\n";
		return;
	}

	auto format = getFormatFromInternalFormat();
	glTextureSubImage2D(id, level, xOffset, yOffset, width, height, format, dataType, data);
}

void Texture::uploadSubData3D(
	const void* data, texture_size xOffset, texture_size yOffset, texture_size zOffset,
	texture_size width, texture_size height, texture_size depth, GLenum dataType, mip_level level)
{
	if (type != GL_TEXTURE_3D)
	{
		std::cerr << "[Texture][uploadSubData3D]: Texture is not 3D type. Actual type: " << type << "\n";
		return;
	}

	if (level < 0 || level >= mipLevels)
	{
		std::cerr << "[Texture][uploadSubData3D]: Invalid mipmap level: " << level
			<< ". Texture has " << (unsigned)mipLevels << " mipLevels.\n";
		return;
	}

	if (xOffset < 0 || yOffset < 0 || zOffset < 0 ||
		width <= 0 || height <= 0 || depth <= 0 ||
		(xOffset + width) > this->width || (yOffset + height) > this->height ||
		(zOffset + depth) > this->depth)
	{
		std::cerr << "[Texture][uploadSubData3D]: Invalid subregion: offset=["
			<< xOffset << "," << yOffset << "," << zOffset << "], size=["
			<< width << "x" << height << "x" << depth << "], texture size=["
			<< this->width << "x" << this->height << "x" << this->depth << "]\n";
		return;
	}

	auto format = getFormatFromInternalFormat();
	glTextureSubImage3D(id, level, xOffset, yOffset, zOffset, width, height, depth, format, dataType, data);
}

void Texture::uploadSubData2DArray(
	const void* data, texture_size xOffset, texture_size yOffset, texture_size layer,
	texture_size width, texture_size height, GLenum dataType, mip_level level)
{
	if (type != GL_TEXTURE_2D_ARRAY)
	{
		std::cerr << "[Texture][uploadSubData2DArray]: Texture is not 2D Array type. Actual type: " << type << "\n";
		return;
	}

	if (level < 0 || level >= mipLevels)
	{
		std::cerr << "[Texture][uploadSubData2DArray]: Invalid mipmap level: " << level
			<< ". Texture has " << (unsigned)mipLevels << " mipLevels.\n";
		return;
	}

	if (xOffset < 0 || yOffset < 0 || layer < 0 ||
		width <= 0 || height <= 0 ||
		(xOffset + width) > this->width || (yOffset + height) > this->height ||
		layer >= this->depth)
	{
		std::cerr << "[Texture][uploadSubData2DArray]: Invalid subregion: xOffset="
			<< xOffset << ", yOffset=" << yOffset << ", layer=" << layer
			<< ", width=" << width << ", height=" << height
			<< ", texture size=" << this->width << "x" << this->height
			<< "x" << this->depth << "\n";
		return;
	}

	auto format = getFormatFromInternalFormat();
	// Upload a single layer (depth = 1)
	glTextureSubImage3D(id, level, xOffset, yOffset, layer, width, height, 1, format, dataType, data);
}

void Texture::generateMipmaps()
{
	if (mipLevels > 1)
	{
		glGenerateTextureMipmap(id);
	}
}

void Texture::setParameters(const Parameters& params)
{
	//// Check what have changed and apply it
	//if (parametrs.minFilter != params.minFilter)	glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, params.minFilter);
	//if (parametrs.magFilter != params.magFilter)	glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, params.magFilter);
	//if (parametrs.wrapS != params.wrapS)			glTextureParameteri(id, GL_TEXTURE_WRAP_S, params.wrapS);
	//if (parametrs.wrapT != params.wrapT)			glTextureParameteri(id, GL_TEXTURE_WRAP_T, params.wrapT);
	//if (parametrs.wrapR != params.wrapR)			glTextureParameteri(id, GL_TEXTURE_WRAP_R, params.wrapR);
	//if (parametrs.anisotropy != params.anisotropy)	glTextureParameteri(id, GL_TEXTURE_MAX_ANISOTROPY, params.anisotropy);
	//
	//// Set new parametrs
	//parametrs = params;

	parametrs = params;
	parametrs.anisotropy = fminf(globalData.maxAnisotropy, fmaxf(1.0f, parametrs.anisotropy));

	applyParametrs();
}

void Texture::bind() const
{
	glBindTexture(type, id);
}

void Texture::bind(GLenum target) const
{
	glBindTexture(target, id);
}

void Texture::unbind() const
{
	glBindTexture(type, 0);
}

void Texture::unbind(GLenum target)
{
	glBindTexture(target, 0);
}

void Texture::bindUnit(GLuint unit) const
{
	glBindTextureUnit(unit, id);
}

void Texture::initHandle()
{
	if (handle == 0 && id != 0)
	{
#if TEXTURE_EXTENSION_WARNINGS
		if (!globalData.extensions.bindless)
		{
			std::cerr << "[Texture][initHandle]: Warning: Initializing texture handle without bindless texture extension support.\n";
			return;
		}
#endif
		handle = glGetTextureHandleARB(id);
	}
}

void Texture::makeResident()
{
	if (handle != 0 && !resident)
	{
#if TEXTURE_EXTENSION_WARNINGS
		if (!globalData.extensions.bindless)
		{
			std::cerr << "[Texture][makeNonResident]: Warning: Making texture resident without bindless texture extension support.\n";
			return;
		}
#endif
		glMakeTextureHandleResidentARB(handle);
		resident = true;
	}
}

void Texture::makeNonResident()
{
	if (handle != 0 && resident)
	{
#if TEXTURE_EXTENSION_WARNINGS
		if (!globalData.extensions.bindless)
		{
			std::cerr << "[Texture][makeNonResident]: Warning: Making texture non-resident without bindless texture extension support.\n";
			return;
		}
#endif
		glMakeTextureHandleNonResidentARB(handle);
		resident = false;
	}
}

GLenum Texture::getFormatFromInternalFormat() const
{
	switch (internalFormat)
	{
		// Red formats (float/unsigned normalized)
	case GL_R8:
	case GL_R16:
	case GL_R16F:
	case GL_R32F:
	case GL_COMPRESSED_RED:
	case GL_COMPRESSED_RED_RGTC1:
		return GL_RED;

		// Red integer formats
	case GL_R8I:
	case GL_R8UI:
	case GL_R16I:
	case GL_R16UI:
	case GL_R32I:
	case GL_R32UI:
		return GL_RED_INTEGER;

		// RG formats (float/unsigned normalized)
	case GL_RG8:
	case GL_RG16:
	case GL_RG16F:
	case GL_RG32F:
	case GL_COMPRESSED_RG:
	case GL_COMPRESSED_RG_RGTC2:
		return GL_RG;

		// RG integer formats
	case GL_RG8I:
	case GL_RG8UI:
	case GL_RG16I:
	case GL_RG16UI:
	case GL_RG32I:
	case GL_RG32UI:
		return GL_RG_INTEGER;

		// RGB formats (float/unsigned normalized)
	case GL_R3_G3_B2:
	case GL_RGB4:
	case GL_RGB5:
	case GL_RGB8:
	case GL_RGB10:
	case GL_RGB12:
	case GL_RGB16:
	case GL_RGB16F:
	case GL_RGB32F:
	case GL_R11F_G11F_B10F:
	case GL_RGB9_E5:
	case GL_COMPRESSED_RGB:
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		return GL_RGB;

		// RGB integer formats
	case GL_RGB8I:
	case GL_RGB8UI:
	case GL_RGB16I:
	case GL_RGB16UI:
	case GL_RGB32I:
	case GL_RGB32UI:
		return GL_RGB_INTEGER;

		// BGR format (no integer BGR in standard OpenGL)
	case GL_BGR:
		return GL_BGR;

		// RGBA formats (float/unsigned normalized)
	case GL_RGBA2:
	case GL_RGBA4:
	case GL_RGB5_A1:
	case GL_RGBA8:
	case GL_RGB10_A2:
	case GL_RGBA12:
	case GL_RGBA16:
	case GL_RGBA16F:
	case GL_RGBA32F:
	case GL_COMPRESSED_RGBA:
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		return GL_RGBA;

		// RGBA integer formats
	case GL_RGBA8I:
	case GL_RGBA8UI:
	case GL_RGB10_A2UI:
	case GL_RGBA16I:
	case GL_RGBA16UI:
	case GL_RGBA32I:
	case GL_RGBA32UI:
		return GL_RGBA_INTEGER;

		// BGRA format (no integer BGRA in standard OpenGL)
	case GL_BGRA:
		return GL_BGRA;

		// Depth formats
	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_COMPONENT32:
	case GL_DEPTH_COMPONENT32F:
		return GL_DEPTH_COMPONENT;

		// Depth-stencil formats
	case GL_DEPTH24_STENCIL8:
	case GL_DEPTH32F_STENCIL8:
		return GL_DEPTH_STENCIL;

		// Stencil format
	case GL_STENCIL_INDEX8:
		return GL_STENCIL_INDEX;

		// Default to RGBA for unknown formats
	default:
		std::cerr << "[Texture][getFormatFromInternalFormat]: Unknown internal format: " << internalFormat
			<< ", defaulting to GL_RGBA\n";
		return GL_RGBA;
	}
}