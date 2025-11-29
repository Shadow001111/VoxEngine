#pragma once
#include <glad/glad.h>
#include <unordered_map>
#include <vector>
#include <string>

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
        GLuint textureID;
        AttachmentType type;
        GLenum internalFormat;
        GLenum format;
        GLenum dataType;
        int attachmentPoint;
        bool isExternal;
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
    void linkColorTexture(const std::string& name, GLuint textureID, int attachmentPoint = -1);
    void linkDepthTexture(const std::string& name, GLuint textureID);
    void linkStencilTexture(const std::string& name, GLuint textureID);
    void linkDepthStencilTexture(const std::string& name, GLuint textureID);

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
    GLuint getTexture(const std::string& name) const;
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
    void createTexture(Attachment& attachment, GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT);
};