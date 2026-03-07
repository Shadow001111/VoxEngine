#include "Texture.h"
#include <iostream>


namespace TextureCompression
{
	static Support g_support;

	void init()
	{
		g_support.s3tc = static_cast<bool>(GLAD_GL_EXT_texture_compression_s3tc);
		g_support.rgtc = static_cast<bool>(GLAD_GL_ARB_texture_compression_rgtc);
		g_support.bptc = static_cast<bool>(GLAD_GL_ARB_texture_compression_bptc);
		g_support.astc = static_cast<bool>(GLAD_GL_KHR_texture_compression_astc_ldr);
	}

	const Support& getSupport()
	{
		return g_support;
	}

	bool isFormatSupported(Format format)
	{
		switch (format)
		{
		case Format::NONE:
		case Format::AUTO:
			return true;

		case Format::BC1:
		case Format::BC3:
			return g_support.s3tc;

		case Format::BC4:
		case Format::BC4_SIGNED:
		case Format::BC5:
		case Format::BC5_SIGNED:
			return g_support.rgtc;

		case Format::BC6H:
		case Format::BC6H_SIGNED:
		case Format::BC7:
			return g_support.bptc;

		case Format::ASTC_4x4:
		case Format::ASTC_6x6:
		case Format::ASTC_8x8:
			return g_support.astc;

		default:
			return false;
		}
	}

	GLenum resolveInternalFormat(Format format, Channels channels, bool isHDR)
	{
		switch (format)
		{
		case Format::NONE:
			return GL_NONE;

		case Format::BC1:
			if (!g_support.s3tc)
			{
				std::cerr << "[TextureCompression]: BC1 (DXT1) requested but S3TC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return (channels == Channels::RGBA)
				? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
				: GL_COMPRESSED_RGB_S3TC_DXT1_EXT;

		case Format::BC3:
			if (!g_support.s3tc)
			{
				std::cerr << "[TextureCompression]: BC3 (DXT5) requested but S3TC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;

		case Format::BC4:
			if (!g_support.rgtc)
			{
				std::cerr << "[TextureCompression]: BC4 requested but RGTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_RED_RGTC1;

		case Format::BC4_SIGNED:
			if (!g_support.rgtc)
			{
				std::cerr << "[TextureCompression]: BC4_SIGNED requested but RGTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_SIGNED_RED_RGTC1;

		case Format::BC5:
			if (!g_support.rgtc)
			{
				std::cerr << "[TextureCompression]: BC5 requested but RGTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_RG_RGTC2;

		case Format::BC5_SIGNED:
			if (!g_support.rgtc)
			{
				std::cerr << "[TextureCompression]: BC5_SIGNED requested but RGTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_SIGNED_RG_RGTC2;

		case Format::BC6H:
			if (!g_support.bptc)
			{
				std::cerr << "[TextureCompression]: BC6H requested but BPTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;

		case Format::BC6H_SIGNED:
			if (!g_support.bptc)
			{
				std::cerr << "[TextureCompression]: BC6H_SIGNED requested but BPTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;

		case Format::BC7:
			if (!g_support.bptc)
			{
				std::cerr << "[TextureCompression]: BC7 requested but BPTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_RGBA_BPTC_UNORM;

		case Format::ASTC_4x4:
			if (!g_support.astc)
			{
				std::cerr << "[TextureCompression]: ASTC_4x4 requested but ASTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;

		case Format::ASTC_6x6:
			if (!g_support.astc)
			{
				std::cerr << "[TextureCompression]: ASTC_6x6 requested but ASTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_RGBA_ASTC_6x6_KHR;

		case Format::ASTC_8x8:
			if (!g_support.astc)
			{
				std::cerr << "[TextureCompression]: ASTC_8x8 requested but ASTC not supported. Falling back to NONE.\n";
				return GL_NONE;
			}
			return GL_COMPRESSED_RGBA_ASTC_8x8_KHR;

		case Format::AUTO:
			break;

		default:
			std::cerr << "[TextureCompression]: Unknown Format value. Falling back to NONE.\n";
			return GL_NONE;
		}

		// AUTO resolution rules (priority: BPTC > RGTC/S3TC > ASTC > generic)
		switch (channels)
		{
		case Channels::R:
			if (g_support.rgtc) return GL_COMPRESSED_RED_RGTC1;
			return GL_COMPRESSED_RED;

		case Channels::RG:
			if (g_support.rgtc) return GL_COMPRESSED_RG_RGTC2;
			return GL_COMPRESSED_RG;

		case Channels::RGB:
			if (isHDR)
			{
				if (g_support.bptc) return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
			}
			if (g_support.bptc)  return GL_COMPRESSED_RGBA_BPTC_UNORM;   // BC7 handles RGB too
			if (g_support.s3tc)  return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
			if (g_support.astc)  return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
			return GL_COMPRESSED_RGB;

		case Channels::RGBA:
			if (g_support.bptc)  return GL_COMPRESSED_RGBA_BPTC_UNORM;
			if (g_support.s3tc)  return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			if (g_support.astc)  return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
			return GL_COMPRESSED_RGBA;
		}

		return GL_COMPRESSED_RGBA; // Unreachable, but keeps compilers happy
	}

	std::size_t calcCompressedSize(Format format, int width, int height)
	{
		// Block-compressed formats always work on 4x4 pixel blocks.
		// Round dimensions up to the next multiple of 4.
		int blocksX = (width + 3) / 4;
		int blocksY = (height + 3) / 4;

		switch (format)
		{
		case Format::BC1:
		case Format::BC4:
		case Format::BC4_SIGNED:
			// 8 bytes per 4x4 block
			return static_cast<std::size_t>(blocksX * blocksY * 8);

		case Format::BC3:
		case Format::BC5:
		case Format::BC5_SIGNED:
		case Format::BC6H:
		case Format::BC6H_SIGNED:
		case Format::BC7:
			// 16 bytes per 4x4 block
			return static_cast<std::size_t>(blocksX * blocksY * 16);

		case Format::ASTC_4x4:
			// 16 bytes per 4x4 block
			return static_cast<std::size_t>(blocksX * blocksY * 16);

		case Format::ASTC_6x6:
		{
			int bx = (width + 5) / 6;
			int by = (height + 5) / 6;
			return static_cast<std::size_t>(bx * by * 16);
		}

		case Format::ASTC_8x8:
		{
			int bx = (width + 7) / 8;
			int by = (height + 7) / 8;
			return static_cast<std::size_t>(bx * by * 16);
		}

		default:
			return 0;
		}
	}

	const char* getName(Format format)
	{
		switch (format)
		{
		case Format::NONE:        return "NONE";
		case Format::AUTO:        return "AUTO";
		case Format::BC1:         return "BC1 (DXT1)";
		case Format::BC3:         return "BC3 (DXT5)";
		case Format::BC4:         return "BC4";
		case Format::BC4_SIGNED:  return "BC4_SIGNED";
		case Format::BC5:         return "BC5";
		case Format::BC5_SIGNED:  return "BC5_SIGNED";
		case Format::BC6H:        return "BC6H";
		case Format::BC6H_SIGNED: return "BC6H_SIGNED";
		case Format::BC7:         return "BC7";
		case Format::ASTC_4x4:    return "ASTC_4x4";
		case Format::ASTC_6x6:    return "ASTC_6x6";
		case Format::ASTC_8x8:    return "ASTC_8x8";
		default:                  return "UNKNOWN";
		}
	}
}

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

	// Compression formats support
	TextureCompression::init();
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

// TODO: Either fall back to uncompressed forma or return success status
void Texture::create2DCompressed(texture_size width, texture_size height, TextureCompression::Channels channels, TextureCompression::Format compression, mip_level mipLevels, bool isHDR)
{
	GLenum fmt = TextureCompression::resolveInternalFormat(compression, channels, isHDR);

	if (fmt == GL_NONE)
	{
		std::cerr << "[Texture][create2DCompressed]: Could not resolve a compressed internal format "
			<< "for compression=" << TextureCompression::getName(compression)
			<< ". The texture will not be created.\n";
		return;
	}

	create2D(width, height, fmt, mipLevels);
}

void Texture::create2DArrayCompressed(texture_size width, texture_size height, texture_size layers, TextureCompression::Channels channels, TextureCompression::Format compression, mip_level mipLevels, bool isHDR)
{
	GLenum fmt = TextureCompression::resolveInternalFormat(compression, channels, isHDR);

	if (fmt == GL_NONE)
	{
		std::cerr << "[Texture][create2DArrayCompressed]: Could not resolve a compressed internal format "
			<< "for compression=" << TextureCompression::getName(compression)
			<< ". The texture will not be created.\n";
		return;
	}

	create2DArray(width, height, layers, fmt, mipLevels);
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

void Texture::uploadCompressedData(const void* data, std::size_t dataSize, mip_level level)
{
	if (!isCompressed())
	{
		std::cerr << "[Texture][uploadCompressedData]: Texture internal format is not compressed. "
			"Use uploadData() for uncompressed formats.\n";
		return;
	}
	if (level >= mipLevels)
	{
		std::cerr << "[Texture][uploadCompressedData]: Invalid mipmap level: " << (unsigned)level
			<< ". Texture has " << (unsigned)mipLevels << " mipLevels.\n";
		return;
	}
	if (!data || dataSize == 0)
	{
		std::cerr << "[Texture][uploadCompressedData]: Null or zero-size data.\n";
		return;
	}

	// Mip-level dimensions
	int mipWidth = std::max(1, (int)width >> level);
	int mipHeight = std::max(1, (int)height >> level);

	switch (type)
	{
	case GL_TEXTURE_2D:
		glCompressedTextureSubImage2D(
			id, level,
			0, 0, mipWidth, mipHeight,
			internalFormat, static_cast<GLsizei>(dataSize), data);
		break;

	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_3D:
	{
		int mipDepth = std::max(1, (int)depth >> level);
		glCompressedTextureSubImage3D(
			id, level,
			0, 0, 0, mipWidth, mipHeight, mipDepth,
			internalFormat, static_cast<GLsizei>(dataSize), data);
		break;
	}

	default:
		std::cerr << "[Texture][uploadCompressedData]: Unsupported texture type for compressed upload: "
			<< type << "\n";
		break;
	}
}

void Texture::uploadCompressedSubData2D(const void* data, std::size_t dataSize, texture_size xOffset, texture_size yOffset, texture_size width, texture_size height, mip_level level)
{
	if (type != GL_TEXTURE_2D)
	{
		std::cerr << "[Texture][uploadCompressedSubData2D]: Texture is not 2D type. Actual type: " << type << "\n";
		return;
	}
	if (!isCompressed())
	{
		std::cerr << "[Texture][uploadCompressedSubData2D]: Texture internal format is not compressed.\n";
		return;
	}
	if (level >= mipLevels)
	{
		std::cerr << "[Texture][uploadCompressedSubData2D]: Invalid mipmap level: " << (unsigned)level
			<< ". Texture has " << (unsigned)mipLevels << " mipLevels.\n";
		return;
	}
	if (!data || dataSize == 0)
	{
		std::cerr << "[Texture][uploadCompressedSubData2D]: Null or zero-size data.\n";
		return;
	}
	// Offsets for block-compressed formats must be multiples of the block size (4)
	if ((xOffset % 4) != 0 || (yOffset % 4) != 0)
	{
		std::cerr << "[Texture][uploadCompressedSubData2D]: xOffset and yOffset must be "
			"multiples of 4 for block-compressed formats.\n";
		return;
	}

	glCompressedTextureSubImage2D(
		id, level,
		xOffset, yOffset, width, height,
		internalFormat, static_cast<GLsizei>(dataSize), data);
}

void Texture::uploadCompressedSubData2DArray(const void* data, std::size_t dataSize, texture_size xOffset, texture_size yOffset, texture_size layer, texture_size width, texture_size height, mip_level level)
{
	if (type != GL_TEXTURE_2D_ARRAY)
	{
		std::cerr << "[Texture][uploadCompressedSubData2DArray]: Texture is not 2D Array type. Actual type: " << type << "\n";
		return;
	}
	if (!isCompressed())
	{
		std::cerr << "[Texture][uploadCompressedSubData2DArray]: Texture internal format is not compressed.\n";
		return;
	}
	if (level >= mipLevels)
	{
		std::cerr << "[Texture][uploadCompressedSubData2DArray]: Invalid mipmap level: " << (unsigned)level
			<< ". Texture has " << (unsigned)mipLevels << " mipLevels.\n";
		return;
	}
	if (!data || dataSize == 0)
	{
		std::cerr << "[Texture][uploadCompressedSubData2DArray]: Null or zero-size data.\n";
		return;
	}
	if ((xOffset % 4) != 0 || (yOffset % 4) != 0)
	{
		std::cerr << "[Texture][uploadCompressedSubData2DArray]: xOffset and yOffset must be "
			"multiples of 4 for block-compressed formats.\n";
		return;
	}
	if (layer >= this->depth)
	{
		std::cerr << "[Texture][uploadCompressedSubData2DArray]: layer " << layer
			<< " out of range (depth=" << this->depth << ").\n";
		return;
	}

	// Upload a single layer (depth=1)
	glCompressedTextureSubImage3D(
		id, level,
		xOffset, yOffset, layer, width, height, 1,
		internalFormat, static_cast<GLsizei>(dataSize), data);
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
		// ---- Red ----
	case GL_R8:
	case GL_R16:
	case GL_R16F:
	case GL_R32F:
	case GL_COMPRESSED_RED:
	case GL_COMPRESSED_RED_RGTC1:
	case GL_COMPRESSED_SIGNED_RED_RGTC1:
		return GL_RED;

	case GL_R8I:
	case GL_R8UI:
	case GL_R16I:
	case GL_R16UI:
	case GL_R32I:
	case GL_R32UI:
		return GL_RED_INTEGER;

		// ---- RG ----
	case GL_RG8:
	case GL_RG16:
	case GL_RG16F:
	case GL_RG32F:
	case GL_COMPRESSED_RG:
	case GL_COMPRESSED_RG_RGTC2:
	case GL_COMPRESSED_SIGNED_RG_RGTC2:
		return GL_RG;

	case GL_RG8I:
	case GL_RG8UI:
	case GL_RG16I:
	case GL_RG16UI:
	case GL_RG32I:
	case GL_RG32UI:
		return GL_RG_INTEGER;

		// ---- RGB ----
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
	case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
	case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
		return GL_RGB;

	case GL_RGB8I:
	case GL_RGB8UI:
	case GL_RGB16I:
	case GL_RGB16UI:
	case GL_RGB32I:
	case GL_RGB32UI:
		return GL_RGB_INTEGER;

	case GL_BGR:
		return GL_BGR;

		// ---- RGBA ----
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
	case GL_COMPRESSED_SRGB:
	case GL_COMPRESSED_SRGB_ALPHA:
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
	case GL_COMPRESSED_RGBA_BPTC_UNORM:
	case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
	case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
	case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
	case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
	case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
	case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
	case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
		return GL_RGBA;

	case GL_RGBA8I:
	case GL_RGBA8UI:
	case GL_RGB10_A2UI:
	case GL_RGBA16I:
	case GL_RGBA16UI:
	case GL_RGBA32I:
	case GL_RGBA32UI:
		return GL_RGBA_INTEGER;

	case GL_BGRA:
		return GL_BGRA;

		// ---- Depth ----
	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_COMPONENT32:
	case GL_DEPTH_COMPONENT32F:
		return GL_DEPTH_COMPONENT;

		// ---- Depth-stencil ----
	case GL_DEPTH24_STENCIL8:
	case GL_DEPTH32F_STENCIL8:
		return GL_DEPTH_STENCIL;

		// ---- Stencil ----
	case GL_STENCIL_INDEX8:
		return GL_STENCIL_INDEX;

	default:
		std::cerr << "[Texture][getFormatFromInternalFormat]: Unknown internal format: "
			<< internalFormat << ", defaulting to GL_RGBA\n";
		return GL_RGBA;
	}
}

const char* Texture::getInternalFormatName() const
{
	switch (internalFormat)
	{
	case GL_NONE: return "GL_NONE";

		// ---- Red (uncompressed) ----
	case GL_R8:    return "GL_R8";
	case GL_R8I:   return "GL_R8I";
	case GL_R8UI:  return "GL_R8UI";
	case GL_R16:   return "GL_R16";
	case GL_R16I:  return "GL_R16I";
	case GL_R16UI: return "GL_R16UI";
	case GL_R16F:  return "GL_R16F";
	case GL_R32I:  return "GL_R32I";
	case GL_R32UI: return "GL_R32UI";
	case GL_R32F:  return "GL_R32F";

		// ---- RG (uncompressed) ----
	case GL_RG8:    return "GL_RG8";
	case GL_RG8I:   return "GL_RG8I";
	case GL_RG8UI:  return "GL_RG8UI";
	case GL_RG16:   return "GL_RG16";
	case GL_RG16I:  return "GL_RG16I";
	case GL_RG16UI: return "GL_RG16UI";
	case GL_RG16F:  return "GL_RG16F";
	case GL_RG32I:  return "GL_RG32I";
	case GL_RG32UI: return "GL_RG32UI";
	case GL_RG32F:  return "GL_RG32F";

		// ---- RGB (uncompressed) ----
	case GL_R3_G3_B2:    return "GL_R3_G3_B2";
	case GL_RGB4:        return "GL_RGB4";
	case GL_RGB5:        return "GL_RGB5";
	case GL_RGB8:        return "GL_RGB8";
	case GL_RGB8I:       return "GL_RGB8I";
	case GL_RGB8UI:      return "GL_RGB8UI";
	case GL_RGB10:       return "GL_RGB10";
	case GL_RGB12:       return "GL_RGB12";
	case GL_RGB16:       return "GL_RGB16";
	case GL_RGB16I:      return "GL_RGB16I";
	case GL_RGB16UI:     return "GL_RGB16UI";
	case GL_RGB16F:      return "GL_RGB16F";
	case GL_RGB32I:      return "GL_RGB32I";
	case GL_RGB32UI:     return "GL_RGB32UI";
	case GL_RGB32F:      return "GL_RGB32F";
	case GL_R11F_G11F_B10F: return "GL_R11F_G11F_B10F";
	case GL_RGB9_E5:     return "GL_RGB9_E5";

		// ---- RGBA (uncompressed) ----
	case GL_RGBA2:    return "GL_RGBA2";
	case GL_RGBA4:    return "GL_RGBA4";
	case GL_RGB5_A1:  return "GL_RGB5_A1";
	case GL_RGBA8:    return "GL_RGBA8";
	case GL_RGBA8I:   return "GL_RGBA8I";
	case GL_RGBA8UI:  return "GL_RGBA8UI";
	case GL_RGB10_A2: return "GL_RGB10_A2";
	case GL_RGB10_A2UI: return "GL_RGB10_A2UI";
	case GL_RGBA12:   return "GL_RGBA12";
	case GL_RGBA16:   return "GL_RGBA16";
	case GL_RGBA16I:  return "GL_RGBA16I";
	case GL_RGBA16UI: return "GL_RGBA16UI";
	case GL_RGBA16F:  return "GL_RGBA16F";
	case GL_RGBA32I:  return "GL_RGBA32I";
	case GL_RGBA32UI: return "GL_RGBA32UI";
	case GL_RGBA32F:  return "GL_RGBA32F";

		// ---- BGR / BGRA ----
	case GL_BGR:  return "GL_BGR";
	case GL_BGRA: return "GL_BGRA";

		// ---- Depth ----
	case GL_DEPTH_COMPONENT16:  return "GL_DEPTH_COMPONENT16";
	case GL_DEPTH_COMPONENT24:  return "GL_DEPTH_COMPONENT24";
	case GL_DEPTH_COMPONENT32:  return "GL_DEPTH_COMPONENT32";
	case GL_DEPTH_COMPONENT32F: return "GL_DEPTH_COMPONENT32F";

		// ---- Depth-stencil ----
	case GL_DEPTH24_STENCIL8:   return "GL_DEPTH24_STENCIL8";
	case GL_DEPTH32F_STENCIL8:  return "GL_DEPTH32F_STENCIL8";

		// ---- Stencil ----
	case GL_STENCIL_INDEX8: return "GL_STENCIL_INDEX8";

		// ---- Generic compressed ----
	case GL_COMPRESSED_RED:        return "GL_COMPRESSED_RED";
	case GL_COMPRESSED_RG:         return "GL_COMPRESSED_RG";
	case GL_COMPRESSED_RGB:        return "GL_COMPRESSED_RGB";
	case GL_COMPRESSED_RGBA:       return "GL_COMPRESSED_RGBA";
	case GL_COMPRESSED_SRGB:       return "GL_COMPRESSED_SRGB";
	case GL_COMPRESSED_SRGB_ALPHA: return "GL_COMPRESSED_SRGB_ALPHA";

		// ---- RGTC (BC4 / BC5) ----
	case GL_COMPRESSED_RED_RGTC1:        return "GL_COMPRESSED_RED_RGTC1 (BC4)";
	case GL_COMPRESSED_SIGNED_RED_RGTC1: return "GL_COMPRESSED_SIGNED_RED_RGTC1 (BC4 signed)";
	case GL_COMPRESSED_RG_RGTC2:         return "GL_COMPRESSED_RG_RGTC2 (BC5)";
	case GL_COMPRESSED_SIGNED_RG_RGTC2:  return "GL_COMPRESSED_SIGNED_RG_RGTC2 (BC5 signed)";

		// ---- S3TC / DXT (BC1 / BC3) ----
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:  return "GL_COMPRESSED_RGB_S3TC_DXT1_EXT (BC1)";
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: return "GL_COMPRESSED_RGBA_S3TC_DXT1_EXT (BC1 alpha)";
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: return "GL_COMPRESSED_RGBA_S3TC_DXT3_EXT (BC2)";
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: return "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT (BC3)";

		// ---- BPTC (BC6H / BC7) ----
	case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT: return "GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT (BC6H)";
	case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:   return "GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT (BC6H signed)";
	case GL_COMPRESSED_RGBA_BPTC_UNORM:         return "GL_COMPRESSED_RGBA_BPTC_UNORM (BC7)";
	case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:   return "GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM (BC7 sRGB)";

		// ---- ASTC ----
	case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:          return "GL_COMPRESSED_RGBA_ASTC_4x4_KHR";
	case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:          return "GL_COMPRESSED_RGBA_ASTC_5x5_KHR";
	case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:          return "GL_COMPRESSED_RGBA_ASTC_6x6_KHR";
	case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:          return "GL_COMPRESSED_RGBA_ASTC_8x8_KHR";
	case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:        return "GL_COMPRESSED_RGBA_ASTC_10x10_KHR";
	case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:        return "GL_COMPRESSED_RGBA_ASTC_12x12_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:  return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:  return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:  return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR";

	default:
	{
		// Return a persistent buffer with the hex value.
		// Using a small ring of static buffers so consecutive calls with
		// different unknown formats do not overwrite each other.
		static char bufs[4][24];
		static int  slot = 0;
		slot = (slot + 1) % 4;
		std::snprintf(bufs[slot], sizeof(bufs[slot]), "UNKNOWN (0x%X)", internalFormat);
		return bufs[slot];
	}
	}
}

bool Texture::isFormatCompressed(GLenum internalFormat)
{
	switch (internalFormat)
	{
		// Generic compressed
	case GL_COMPRESSED_RED:
	case GL_COMPRESSED_RG:
	case GL_COMPRESSED_RGB:
	case GL_COMPRESSED_RGBA:
	case GL_COMPRESSED_SRGB:
	case GL_COMPRESSED_SRGB_ALPHA:
		// RGTC (BC4/BC5)
	case GL_COMPRESSED_RED_RGTC1:
	case GL_COMPRESSED_SIGNED_RED_RGTC1:
	case GL_COMPRESSED_RG_RGTC2:
	case GL_COMPRESSED_SIGNED_RG_RGTC2:
		// S3TC / DXT (BC1/BC3)
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		// BPTC (BC6H/BC7)
	case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
	case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
	case GL_COMPRESSED_RGBA_BPTC_UNORM:
	case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
		// ASTC
	case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
	case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
	case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
	case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
	case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
	case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
		return true;

	default:
		return false;
	}
}

bool Texture::isCompressed() const
{
	return isFormatCompressed(internalFormat);
}
