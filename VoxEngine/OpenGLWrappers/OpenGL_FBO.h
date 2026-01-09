#pragma once
#include "OpenGL_Texture.h"
#include "robin_hood.h"
#include <string>
#include <optional>

class OpenGL_FBO
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
        OpenGL_Texture texture;
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
    OpenGL_FBO() = default;
    ~OpenGL_FBO();

    // Delete copy constructor/assignment
    OpenGL_FBO(const OpenGL_FBO&) = delete;
    OpenGL_FBO& operator=(const OpenGL_FBO&) = delete;

    // Move constructor/assignment
    OpenGL_FBO(OpenGL_FBO&& other) noexcept;
    OpenGL_FBO& operator=(OpenGL_FBO&& other) noexcept;

    // Creation
    void create(int width, int height);

    // Attachment management
    void createColorAttachment(const std::string& name, GLenum internalFormat = GL_RGBA8,
        GLenum format = GL_RGBA, GLenum dataType = GL_UNSIGNED_BYTE,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    void createDepthAttachment(const std::string& name, GLenum internalFormat = GL_DEPTH_COMPONENT32F,
        GLenum format = GL_DEPTH_COMPONENT, GLenum dataType = GL_FLOAT,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    void createStencilAttachment(const std::string& name, GLenum internalFormat = GL_STENCIL_INDEX8,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    void createDepthStencilAttachment(const std::string& name, GLenum internalFormat = GL_DEPTH24_STENCIL8,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    void createStandaloneTextureAttachment(const std::string& name, GLenum internalFormat = GL_RGBA8,
        GLenum format = GL_RGBA, GLenum dataType = GL_UNSIGNED_BYTE,
        float resolutionFactor = 1.0f,
        GLenum minFilter = GL_NEAREST, GLenum magFilter = GL_NEAREST,
        GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);

    // Remove attachments
    void removeAttachment(const std::string& name);

    // FBO operations
    void bind() const;
    static void unbind();

    void setDrawBuffers(const std::vector<std::string>& attachmentNames);
    void resize(int newWidth, int newHeight);
    void clear() const;

    // Not for color attachments
    void clearAttachment(const std::string& name, const float* clearValue) const;

    // For color attachments which are currently draw buffers
    void clearDrawBuffer(const std::string& name, const float* clearValue) const;

    // Texture access
    std::optional<OpenGL_Texture*> getTexture(const std::string& name);
    std::optional<const OpenGL_Texture*> getTexture(const std::string& name) const;
    bool hasTexture(const std::string& name) const;
    void bindTexture(const std::string& name, GLuint textureUnit) const;

    // Getters
    bool isComplete() const;
    GLuint getID() const { return id; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
private:
    GLenum getAttachmentPoint(AttachmentType type);
    void createAndAttachTexture(Attachment& attachment, GLenum internalFormat, GLenum format, GLenum dataType,
        GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT);
    void createStandaloneTexture(Attachment& attachment, GLenum internalFormat, GLenum format, GLenum dataType,
        GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT);
};