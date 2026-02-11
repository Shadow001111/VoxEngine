#pragma once
#include <glad/glad.h>
#include <cstdint>

//namespace TextureCompression
//{
//	// Simple format picker for voxel games:
//	// - BC7 (BPTC): Best for RGBA textures
//	// - BC5 (RGTC): Best for normal maps (2 channels)
//	// - BC4 (RGTC): Best for single-channel
//	// - DXT5: Fallback for RGBA when BC7 not available
//	// - DXT1: For RGB textures (no alpha)
//
//	struct CompressionSupport
//	{
//		bool s3tc = false;    // DXT1/3/5
//		bool rgtc = false;    // BC4/5
//		bool bptc = false;    // BC6/7
//		bool astc = false;    // ASTC
//	};
//
//	void setCompressionFormats();
//	GLenum getBestFormat(int channels, GLenum valueType);
//	GLenum getBestCompressedFormat(int channels, GLenum valueType);
//}

// TODO: Add method to copy GPU data from texture to other texture
class Texture
{
	struct Extensions
	{
		bool bindless = false;
	};

	struct GlobalData
	{
		Extensions extensions;

		GLfloat maxAnisotropy = 0;
	};
public:
	struct Parameters
	{
		GLenum minFilter = GL_NEAREST;
		GLenum magFilter = GL_NEAREST;
		GLenum wrapS = GL_CLAMP_TO_EDGE;
		GLenum wrapT = GL_CLAMP_TO_EDGE;
		GLenum wrapR = GL_CLAMP_TO_EDGE;

		float anisotropy = 1.0f;
	};
private:
	static GlobalData globalData;

	GLuint id = 0;
	GLenum type = 0;
	GLenum internalFormat = 0;

	Parameters parametrs;

	using texture_size = uint16_t;
	using mip_level = uint8_t;

	texture_size width = 0;
	texture_size height = 0;
	texture_size depth = 0;
	mip_level mipLevels = 1;

	bool resident = false;
	GLuint64 handle = 0;

	void applyParametrs() const;
public:
	Texture() = default;
	~Texture();

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;

	static void initGlobalData();

	// Texture creation functions for different types
	void create1D(texture_size width, GLenum internalFormat, mip_level mipLevels = 1);
	void create2D(texture_size width, texture_size height, GLenum internalFormat, mip_level mipLevels = 1);
	void create3D(texture_size width, texture_size height, texture_size depth, GLenum internalFormat, mip_level mipLevels = 1);
	void create2DArray(texture_size width, texture_size height, texture_size layers, GLenum internalFormat, mip_level mipLevels = 1);

	// Texture resizing functions (it deletes old texture and creates new, since texture is immutable)
	void recreate1D(texture_size width);
	void recreate2D(texture_size width, texture_size height);
	void recreate3D(texture_size width, texture_size height, texture_size depth);

	// Data upload functions
	void uploadData(const void* data, GLenum dataType, mip_level level = 0);

	void uploadSubData1D(
		const void* data, texture_size xOffset, texture_size width,
		GLenum dataType, mip_level level = 0);
	void uploadSubData2D(
		const void* data, texture_size xOffset, texture_size yOffset,
		texture_size width, texture_size height, GLenum dataType, mip_level level = 0);
	void uploadSubData3D(
		const void* data, texture_size xOffset, texture_size yOffset, texture_size zOffset,
		texture_size width, texture_size height, texture_size depth, GLenum dataType, mip_level level = 0);
	void uploadSubData2DArray(
		const void* data, texture_size xOffset, texture_size yOffset, texture_size layer,
		texture_size width, texture_size height, GLenum dataType, mip_level level = 0);

	//
	void generateMipmaps();

	void setParameters(const Parameters& params);

	void bind() const;
	void bind(GLenum target) const;

	void unbind() const;
	static void unbind(GLenum target);

	void bindUnit(GLuint unit) const;

	// Handle managment
	void initHandle();
	void makeResident();
	void makeNonResident();

	// Getters
	GLuint getID() const { return id; }
	GLenum getType() const { return type; }
	GLenum getInternalFormat() const { return internalFormat; }
	texture_size getWidth() const { return width; }
	texture_size getHeight() const { return height; }
	texture_size getDepth() const { return depth; }
	mip_level getMipLevels() const { return mipLevels; }
	GLenum getFormatFromInternalFormat() const;
	bool isResident() const { return resident; }
	GLuint64 getHandle() const { return handle; }

	static const GlobalData& getGlobalData() { return globalData; }
	static const Extensions& getExtensions() { return globalData.extensions; }
};