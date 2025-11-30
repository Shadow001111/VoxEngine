#pragma once
#include <glad/glad.h>
#include <unordered_map>
#include <vector>
#include <string>
#include "OpenGL_Texture.h"

class OpenGL_FBO
{
    enum class AttachmentType
    {
        COLOR,
        DEPTH,
        STENCIL,
        DEPTH_STENCIL
    };

    struct Attachment
    {
        OpenGL_Texture texture;
        AttachmentType type = AttachmentType::COLOR;
        int attachmentPoint = -1;
        bool isExternal = false;

        Attachment(const Attachment&) = delete;
        Attachment& operator=(const Attachment&) = delete;

        Attachment() = default;

        Attachment(Attachment&& other) noexcept;
        Attachment& operator=(Attachment&& other) noexcept;
    };

    GLuint id;
    int width, height;

    std::unordered_map<std::string, Attachment> attachments;
    std::vector<GLenum> drawBuffers;
public:
    OpenGL_FBO(int width, int height);
    ~OpenGL_FBO();

    // Delete copy constructor/assignment
    OpenGL_FBO(const OpenGL_FBO&) = delete;
    OpenGL_FBO& operator=(const OpenGL_FBO&) = delete;

    // Move constructor/assignment
    OpenGL_FBO(OpenGL_FBO&& other) noexcept;
    OpenGL_FBO& operator=(OpenGL_FBO&& other) noexcept;

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

    // Link existing textures
    void linkColorTexture(const std::string& name, OpenGL_Texture& texture, int attachmentPoint = -1);
    void linkDepthTexture(const std::string& name, OpenGL_Texture& texture);
    void linkStencilTexture(const std::string& name, OpenGL_Texture& texture);
    void linkDepthStencilTexture(const std::string& name, OpenGL_Texture& texture);

    // Remove attachments
    void removeAttachment(const std::string& name);

    // FBO operations
    void bind() const;
    void unbind() const;
    static void unbindGlobal();

    void resize(int newWidth, int newHeight);
    void clear() const;
    void clearAttachment(const std::string& name, const float* clearValue) const;

    // Texture access
    OpenGL_Texture& getTexture(const std::string& name);
    const OpenGL_Texture& getTexture(const std::string& name) const;
    bool hasTexture(const std::string& name) const;

    // Bind textures for shader use
    void bindTexture(const std::string& name, GLuint textureUnit) const;

    // Getters
    bool isComplete() const;
    GLuint getID() const { return id; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    const std::vector<GLenum>& getDrawBuffers() const { return drawBuffers; }
private:
    void setupDrawBuffers();
    GLenum getAttachmentPoint(AttachmentType type, int preferredPoint = -1);
    void createTexture(Attachment& attachment, GLenum internalFormat, GLenum format, GLenum dataType,
        GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT);
};