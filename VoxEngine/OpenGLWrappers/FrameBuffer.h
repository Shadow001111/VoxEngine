#pragma once
#include "Texture.h"
#include "robin_hood.h"
#include <string>
#include <optional>

class FrameBuffer
{
    enum class AttachmentType
    {
        COLOR,
        DEPTH,
        STENCIL,
        DEPTH_STENCIL,
        STANDALONE_TEXTURE
    };

    struct Attachment
    {
        Texture texture;
        AttachmentType type = AttachmentType::COLOR;
        GLenum attachmentPoint = -1;
        float resolutionFactor = 1.0f;

        Attachment(const Attachment&) = delete;
        Attachment& operator=(const Attachment&) = delete;

        Attachment() = default;

        Attachment(Attachment&& other) noexcept;
        Attachment& operator=(Attachment&& other) noexcept;
    };

    GLuint id = 0;
    int width = 0, height = 0;

    robin_hood::unordered_flat_map<std::string, Attachment> attachments;
    std::vector<std::string> drawBuffers;
public:
    FrameBuffer() = default;
    ~FrameBuffer();

    // Delete copy constructor/assignment
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;

    // Move constructor/assignment
    FrameBuffer(FrameBuffer&& other) noexcept;
    FrameBuffer& operator=(FrameBuffer&& other) noexcept;

    // Creation
    void create(int width, int height);
    void destroy();

    // Attachment management
    void createColorAttachment(
        const std::string& name, GLenum internalFormat = GL_RGBA8,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    void createDepthAttachment(
        const std::string& name, GLenum internalFormat = GL_DEPTH_COMPONENT32F,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    void createStencilAttachment(
        const std::string& name, GLenum internalFormat = GL_STENCIL_INDEX8,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    void createDepthStencilAttachment(
        const std::string& name, GLenum internalFormat = GL_DEPTH24_STENCIL8,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    void createStandaloneTextureAttachment(
        const std::string& name, GLenum internalFormat = GL_RGBA8,
        float resolutionFactor = 1.0f,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    // Remove attachments
    void removeAttachment(const std::string& name);

    // FBO operations
    void bind() const;
    void bind(GLenum target) const;

    static void unbind();
    static void unbind(GLenum target);

    void setDrawBuffers(const std::vector<std::string>& attachmentNames);
    void resize(int newWidth, int newHeight);

    void blitTo(const FrameBuffer& dstFBO, GLbitfield mask = GL_COLOR_BUFFER_BIT, GLenum filter = GL_NEAREST) const;
    void blitToDefaultFramebuffer(int dstWidth, int dstHeight, GLbitfield mask = GL_COLOR_BUFFER_BIT, GLenum filter = GL_NEAREST) const;

    void clearAttachment(const std::string& name, const float* clearValue) const;
    void clearDrawBuffer(const std::string& name, const float* clearValue) const;

    // Texture access
    std::optional<Texture*> getTexture(const std::string& name);
    std::optional<const Texture*> getTexture(const std::string& name) const;
    bool hasTexture(const std::string& name) const;
    void bindTextureToUnit(const std::string& name, GLuint textureUnit) const;

    // Getters
    bool isComplete() const;
    GLuint getID() const { return id; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
private:
    GLenum getAttachmentPoint(AttachmentType type);
    void createAndAttachTexture(
        Attachment& attachment, GLenum internalFormat,
        GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT);
    void createStandaloneTexture(
        Attachment& attachment, GLenum internalFormat,
        GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT);
};