#pragma once
#include <glad/glad.h>

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

class OpenGL_Texture
{
	GLuint id = 0;
	GLenum type = 0;
	GLenum internalFormat = 0;

	GLenum minFilter = GL_NEAREST;
	GLenum magFilter = GL_NEAREST;
	GLenum wrapS = GL_CLAMP_TO_EDGE;
	GLenum wrapT = GL_CLAMP_TO_EDGE;
	GLenum wrapR = GL_CLAMP_TO_EDGE;

	int width = 0;
	int height = 0;
	int depth = 0;
	int mipLevels = 1;
public:
	OpenGL_Texture() = default;
	~OpenGL_Texture();

	OpenGL_Texture(const OpenGL_Texture&) = delete;
	OpenGL_Texture& operator=(const OpenGL_Texture&) = delete;

	OpenGL_Texture(OpenGL_Texture&& other) noexcept;
	OpenGL_Texture& operator=(OpenGL_Texture&& other) noexcept;

	// Texture creation functions for different types
	void create1D(int width,							  GLenum internalFormat, int mipLevels = 1);
	void create2D(int width, int height,				  GLenum internalFormat, int mipLevels = 1);
	void create3D(int width, int height, int depth,		  GLenum internalFormat, int mipLevels = 1);
	void create2DArray(int width, int height, int layers, GLenum internalFormat, int mipLevels = 1);

	// Texture resizing functions (it deletes old texture and creates new, since texture is immutable)
	void recreate1D(int width);
	void recreate2D(int width, int height);
	void recreate3D(int width, int height, int depth);

	// Data upload functions
	void uploadData(const void* data, GLenum dataType, int level = 0);
	void uploadSubData(const void* data, int xOffset, int yOffset, int zOffset, int width, int height, int depth, GLenum dataType, int level = 0);

	//
	void generateMipmaps();

	void setParameters(GLenum minFilter_, GLenum magFilter_, GLenum wrapS_);
	void setParameters(GLenum minFilter_, GLenum magFilter_, GLenum wrapS_, GLenum wrapT_);
	void setParameters(GLenum minFilter_, GLenum magFilter_, GLenum wrapS_, GLenum wrapT_, GLenum wrapR_);

	void bind() const;
	void bind(GLenum target) const;

	void unbind() const;
	static void unbind(GLenum target);

	void bindUnit(GLuint unit) const;

	// Getters
	GLuint getID() const { return id; }
	GLenum getType() const { return type; }
	GLenum getInternalFormat() const { return internalFormat; }
	int getWidth() const { return width; }
	int getHeight() const { return height; }
	int getDepth() const { return depth; }
	int getMipLevels() const { return mipLevels; }
	GLenum getFormatFromInternalFormat() const;
};