#include "OpenGL_FBO.h"
#include <stdexcept>
#include <iostream>

OpenGL_FBO::OpenGL_FBO(int width, int height) : width(width), height(height), id(0)
{
    glGenFramebuffers(1, &id);
}

OpenGL_FBO::~OpenGL_FBO()
{
    // Only delete textures that we created (not external ones)
    for (const auto& [name, attachment] : attachments)
    {
        if (!attachment.isExternal)
        {
            glDeleteTextures(1, &attachment.textureID);
        }
    }

    if (id)
        glDeleteFramebuffers(1, &id);
}

OpenGL_FBO::OpenGL_FBO(OpenGL_FBO&& other) noexcept
    : id(other.id), width(other.width), height(other.height),
    attachments(std::move(other.attachments)),
    drawBuffers(std::move(other.drawBuffers))
{
    other.id = 0;
    other.width = 0;
    other.height = 0;
}

OpenGL_FBO& OpenGL_FBO::operator=(OpenGL_FBO&& other) noexcept
{
    if (this != &other)
    {
        // Clean up current resources
        for (const auto& [name, attachment] : attachments)
        {
            if (!attachment.isExternal)
            {
                glDeleteTextures(1, &attachment.textureID);
            }
        }
        if (id)
            glDeleteFramebuffers(1, &id);

        // Move resources
        id = other.id;
        width = other.width;
        height = other.height;
        attachments = std::move(other.attachments);
        drawBuffers = std::move(other.drawBuffers);

        // Reset other
        other.id = 0;
        other.width = 0;
        other.height = 0;
    }
    return *this;
}

void OpenGL_FBO::createColorAttachment(const std::string& name, GLenum internalFormat,
    GLenum format, GLenum dataType,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::COLOR;
    attachment.internalFormat = internalFormat;
    attachment.format = format;
    attachment.dataType = dataType;
    attachment.isExternal = false;
    attachment.attachmentPoint = getAttachmentPoint(AttachmentType::COLOR);

    createTexture(attachment, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = attachment;

    setupDrawBuffers();

    if (!isComplete())
        throw std::runtime_error("Framebuffer is not complete after adding color attachment: " + name);
}

void OpenGL_FBO::createDepthAttachment(const std::string& name, GLenum internalFormat,
    GLenum format, GLenum dataType,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::DEPTH;
    attachment.internalFormat = internalFormat;
    attachment.format = format;
    attachment.dataType = dataType;
    attachment.isExternal = false;
    attachment.attachmentPoint = GL_DEPTH_ATTACHMENT;

    createTexture(attachment, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = attachment;

    if (!isComplete())
        throw std::runtime_error("Framebuffer is not complete after adding depth attachment: " + name);
}

void OpenGL_FBO::createStencilAttachment(const std::string& name, GLenum internalFormat,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::STENCIL;
    attachment.internalFormat = internalFormat;
    attachment.format = GL_STENCIL_INDEX;
    attachment.dataType = GL_UNSIGNED_BYTE;
    attachment.isExternal = false;
    attachment.attachmentPoint = GL_STENCIL_ATTACHMENT;

    createTexture(attachment, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = attachment;

    if (!isComplete())
        throw std::runtime_error("Framebuffer is not complete after adding stencil attachment: " + name);
}

void OpenGL_FBO::createDepthStencilAttachment(const std::string& name, GLenum internalFormat,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::DEPTH_STENCIL;
    attachment.internalFormat = internalFormat;
    attachment.format = GL_DEPTH_STENCIL;
    attachment.dataType = GL_UNSIGNED_INT_24_8;
    attachment.isExternal = false;
    attachment.attachmentPoint = GL_DEPTH_STENCIL_ATTACHMENT;

    createTexture(attachment, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = attachment;

    if (!isComplete())
        throw std::runtime_error("Framebuffer is not complete after adding depth-stencil attachment: " + name);
}

void OpenGL_FBO::linkColorTexture(const std::string& name, GLuint textureID, int attachmentPoint)
{
    Attachment attachment;
    attachment.textureID = textureID;
    attachment.type = AttachmentType::COLOR;
    attachment.isExternal = true;
    attachment.attachmentPoint = getAttachmentPoint(AttachmentType::COLOR, attachmentPoint);

    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment.attachmentPoint, GL_TEXTURE_2D, textureID, 0);
    attachments[name] = attachment;

    setupDrawBuffers();

    if (!isComplete())
        throw std::runtime_error("Framebuffer is not complete after linking color texture: " + name);
}

void OpenGL_FBO::linkDepthTexture(const std::string& name, GLuint textureID)
{
    Attachment attachment;
    attachment.textureID = textureID;
    attachment.type = AttachmentType::DEPTH;
    attachment.isExternal = true;
    attachment.attachmentPoint = GL_DEPTH_ATTACHMENT;

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textureID, 0);
    attachments[name] = attachment;

    if (!isComplete())
        throw std::runtime_error("Framebuffer is not complete after linking depth texture: " + name);
}

void OpenGL_FBO::linkStencilTexture(const std::string& name, GLuint textureID)
{
    Attachment attachment;
    attachment.textureID = textureID;
    attachment.type = AttachmentType::STENCIL;
    attachment.isExternal = true;
    attachment.attachmentPoint = GL_STENCIL_ATTACHMENT;

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, textureID, 0);
    attachments[name] = attachment;

    if (!isComplete())
        throw std::runtime_error("Framebuffer is not complete after linking stencil texture: " + name);
}

void OpenGL_FBO::linkDepthStencilTexture(const std::string& name, GLuint textureID)
{
    Attachment attachment;
    attachment.textureID = textureID;
    attachment.type = AttachmentType::DEPTH_STENCIL;
    attachment.isExternal = true;
    attachment.attachmentPoint = GL_DEPTH_STENCIL_ATTACHMENT;

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, textureID, 0);
    attachments[name] = attachment;

    if (!isComplete())
        throw std::runtime_error("Framebuffer is not complete after linking depth-stencil texture: " + name);
}

void OpenGL_FBO::removeAttachment(const std::string& name)
{
    auto it = attachments.find(name);
    if (it != attachments.end())
    {
        // Detach from framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, it->second.attachmentPoint, GL_TEXTURE_2D, 0, 0);

        // Delete texture if we created it
        if (!it->second.isExternal)
        {
            glDeleteTextures(1, &it->second.textureID);
        }

        attachments.erase(it);
        setupDrawBuffers();
    }
}

void OpenGL_FBO::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, id);
}

void OpenGL_FBO::unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGL_FBO::unbindGlobal()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGL_FBO::resize(int newWidth, int newHeight)
{
    if (newWidth == width && newHeight == height)
        return;

    width = newWidth;
    height = newHeight;

    for (auto& [name, attachment] : attachments)
    {
        if (!attachment.isExternal)
        {
            glBindTexture(GL_TEXTURE_2D, attachment.textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, attachment.internalFormat, width, height, 0,
                attachment.format, attachment.dataType, nullptr);
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGL_FBO::clear() const
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void OpenGL_FBO::clearAttachment(const std::string& name, const float* clearValue) const
{
    auto it = attachments.find(name);
    if (it != attachments.end())
    {
        switch (it->second.type)
        {
        case AttachmentType::COLOR:
            //std::cout << it->second.attachmentPoint - GL_COLOR_ATTACHMENT0 << std::endl;
            glClearBufferfv(GL_COLOR, it->second.attachmentPoint - GL_COLOR_ATTACHMENT0, clearValue);
            break;
        case AttachmentType::DEPTH:
            glClearBufferfv(GL_DEPTH, 0, clearValue);
            break;
        case AttachmentType::STENCIL:
        {
            GLint stencilValue = static_cast<GLint>(clearValue[0]);
            glClearBufferiv(GL_STENCIL, 0, &stencilValue);
            break;
        }
        case AttachmentType::DEPTH_STENCIL:
            glClearBufferfi(GL_DEPTH_STENCIL, 0, clearValue[0], static_cast<GLint>(clearValue[1]));
            break;
        }
    }
}

GLuint OpenGL_FBO::getTexture(const std::string& name) const
{
    auto it = attachments.find(name);
    if (it != attachments.end())
        return it->second.textureID;
    return 0;
}

bool OpenGL_FBO::hasTexture(const std::string& name) const
{
    return attachments.find(name) != attachments.end();
}

void OpenGL_FBO::bindTexture(const std::string& name, GLuint textureUnit) const
{
    auto it = attachments.find(name);
    if (it != attachments.end())
    {
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, it->second.textureID);
    }
}

bool OpenGL_FBO::isComplete() const
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE)
    {
        return true;
    }
    std::string errorMsg;
    switch (status)
    {
    case GL_FRAMEBUFFER_UNDEFINED:
        errorMsg = "GL_FRAMEBUFFER_UNDEFINED";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        errorMsg = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        errorMsg = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        errorMsg = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        errorMsg = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        errorMsg = "GL_FRAMEBUFFER_UNSUPPORTED";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        errorMsg = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        errorMsg = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
        break;
    default:
        errorMsg = "Unknown error: " + std::to_string(status);
        break;
    }
    std::cerr << "[OpenGL][Framebuffer]: Framebuffer is not complete: " << errorMsg << std::endl;
    return false;
}

void OpenGL_FBO::setupDrawBuffers()
{
    drawBuffers.clear();

    for (const auto& [name, attachment] : attachments)
    {
        if (attachment.type == AttachmentType::COLOR)
        {
            drawBuffers.push_back(attachment.attachmentPoint);
        }
    }

    if (drawBuffers.empty())
    {
        // If no color attachments, tell OpenGL we're not drawing to any color buffer
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    else
    {
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    }
}

GLenum OpenGL_FBO::getAttachmentPoint(AttachmentType type, int preferredPoint)
{
    if (type != AttachmentType::COLOR)
    {
        switch (type)
        {
        case AttachmentType::DEPTH: return GL_DEPTH_ATTACHMENT;
        case AttachmentType::STENCIL: return GL_STENCIL_ATTACHMENT;
        case AttachmentType::DEPTH_STENCIL: return GL_DEPTH_STENCIL_ATTACHMENT;
        default: return GL_COLOR_ATTACHMENT0;
        }
    }

    // For color attachments, find the next available slot
    if (preferredPoint >= 0)
    {
        return GL_COLOR_ATTACHMENT0 + preferredPoint;
    }

    int maxPoint = -1;
    for (const auto& [name, attachment] : attachments)
    {
        if (attachment.type == AttachmentType::COLOR && (attachment.attachmentPoint - GL_COLOR_ATTACHMENT0) > maxPoint)
        {
            maxPoint = attachment.attachmentPoint - GL_COLOR_ATTACHMENT0;
        }
    }

    return GL_COLOR_ATTACHMENT0 + (maxPoint + 1);
}

void OpenGL_FBO::createTexture(Attachment& attachment, GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT)
{
    glGenTextures(1, &attachment.textureID);
    glBindTexture(GL_TEXTURE_2D, attachment.textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, attachment.internalFormat, width, height, 0,
        attachment.format, attachment.dataType, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);

    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment.attachmentPoint, GL_TEXTURE_2D, attachment.textureID, 0);
}