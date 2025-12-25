#include "OpenGL_FBO.h"

#include "OpenGLDebug.h"

#include <iostream>
#include <stdexcept>

OpenGL_FBO::OpenGL_FBO(int width, int height) : width(width), height(height)
{
    glGenFramebuffers(1, &id);
}

OpenGL_FBO::~OpenGL_FBO()
{
    if (id) glDeleteFramebuffers(1, &id);
}

OpenGL_FBO::OpenGL_FBO(OpenGL_FBO&& other) noexcept
    : id(other.id), width(other.width), height(other.height),
    attachments(std::move(other.attachments))
{
    other.id = 0;
    other.width = 0;
    other.height = 0;
}

OpenGL_FBO& OpenGL_FBO::operator=(OpenGL_FBO&& other) noexcept
{
    if (this != &other)
    {
        if (id) glDeleteFramebuffers(1, &id);

        id = other.id;
        width = other.width;
        height = other.height;
        attachments = std::move(other.attachments);

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
    attachment.attachmentPoint = getAttachmentPoint(AttachmentType::COLOR);

    createTexture(attachment, internalFormat, format, dataType, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);
}

void OpenGL_FBO::createDepthAttachment(const std::string& name, GLenum internalFormat,
    GLenum format, GLenum dataType,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::DEPTH;
    attachment.attachmentPoint = GL_DEPTH_ATTACHMENT;

    createTexture(attachment, internalFormat, format, dataType, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);
}

void OpenGL_FBO::createStencilAttachment(const std::string& name, GLenum internalFormat,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::STENCIL;
    attachment.attachmentPoint = GL_STENCIL_ATTACHMENT;

    createTexture(attachment, internalFormat, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
        minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);
}

void OpenGL_FBO::createDepthStencilAttachment(const std::string& name, GLenum internalFormat,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::DEPTH_STENCIL;
    attachment.attachmentPoint = GL_DEPTH_STENCIL_ATTACHMENT;

    createTexture(attachment, internalFormat, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
        minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);
}

void OpenGL_FBO::removeAttachment(const std::string& name)
{
    auto it = attachments.find(name);
    if (it != attachments.end())
    {
        OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

        // Detach from framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, it->second.attachmentPoint, GL_TEXTURE_2D, 0, 0);
        attachments.erase(it);
    }
}

void OpenGL_FBO::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, id);
}

void OpenGL_FBO::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGL_FBO::setDrawBuffers(const std::vector<std::string>& attachmentNames) const
{
    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

    if (attachmentNames.empty())
    {
        // If no attachments specified, disable drawing
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        return;
    }

    std::vector<GLenum> drawBuffers;
    drawBuffers.reserve(attachmentNames.size());

    this->drawBuffers.clear();
    this->drawBuffers.reserve(attachmentNames.size());

    for (const auto& name : attachmentNames)
    {
        auto it = attachments.find(name);
        if (it != attachments.end())
        {
            // Only COLOR attachments can be draw buffers
            if (it->second.type == AttachmentType::COLOR)
            {
                drawBuffers.push_back(it->second.attachmentPoint);
                this->drawBuffers.push_back(name);
            }
            else
            {
                std::cerr << "[OpenGL_FBO][setDrawBuffers]: Attachment '" << name
                    << "' is not a COLOR attachment (type: " << static_cast<int>(it->second.type)
                    << "). Skipping.\n";
            }
        }
        else
        {
            std::cerr << "[OpenGL_FBO][setDrawBuffers]: Attachment '" << name
                << "' not found. Skipping.\n";
        }
    }

    if (drawBuffers.empty())
    {
        // No valid color attachments found
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    else
    {
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    }
}

void OpenGL_FBO::resize(int newWidth, int newHeight)
{
    if (newWidth == width && newHeight == height) return;

    width = newWidth;
    height = newHeight;

    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

    for (auto& [name, attachment] : attachments)
    {
        attachment.texture.resize2D(width, height);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment.attachmentPoint, GL_TEXTURE_2D, attachment.texture.getID(), 0);
    }
}

void OpenGL_FBO::clear() const
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void OpenGL_FBO::clearAttachment(const std::string& name, const float* clearValue) const
{
    auto it = attachments.find(name);
    if (it == attachments.end())
    {
        std::cerr << "[OpenGL_FBO][setDrawBuffers]: Attachment '" << name << "' not found in attachments for clearing\n";
        return;
    }

    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

    bool found = false;
    int index = 0;

    switch (it->second.type)
    {
    case AttachmentType::COLOR:
        for (index = 0; index < drawBuffers.size(); index++)
        {
            if (name == drawBuffers[index])
            {
                found = true;
                break;
            }
        }
        if (found)
        {
            glClearBufferfv(GL_COLOR, index, clearValue);
        }
        else
        {
            std::cerr << "[OpenGL_FBO][setDrawBuffers]: Attachment '" << name << "' not found in draw buffers for clearing\n";
        }
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

std::optional<OpenGL_Texture*> OpenGL_FBO::getTexture(const std::string& name)
{
    auto it = attachments.find(name);
    if (it != attachments.end()) return &it->second.texture;
    return std::nullopt;
}

std::optional<const OpenGL_Texture*> OpenGL_FBO::getTexture(const std::string& name) const
{
    auto it = attachments.find(name);
    if (it != attachments.end()) return &it->second.texture;
    return std::nullopt;
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
        it->second.texture.bind(textureUnit);
    }
}

bool OpenGL_FBO::isComplete() const
{
    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

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
    std::cerr << "[OpenGL][Framebuffer]: Framebuffer is not complete: " << errorMsg << "\n";
    return false;
}

GLenum OpenGL_FBO::getAttachmentPoint(AttachmentType type)
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

    constexpr int ATTACHMENT_POINT_COUNT = 32;
    bool usedAttachments[ATTACHMENT_POINT_COUNT];
    std::fill_n(usedAttachments, sizeof(usedAttachments), false);

    for (const auto& [name, attachment] : attachments)
    {
        if (attachment.type == AttachmentType::COLOR)
        {
            int index = attachment.attachmentPoint - GL_COLOR_ATTACHMENT0;
            if (index >= 0 && index < ATTACHMENT_POINT_COUNT)
                usedAttachments[index] = true;
        }
    }

    // Find first unused attachment point
    for (int i = 0; i < ATTACHMENT_POINT_COUNT; i++)
    {
        if (!usedAttachments[i])
        {
            return GL_COLOR_ATTACHMENT0 + i;
        }
    }

    std::cerr << "[OpenGL_FBO][getAttachmentPoint]: No available color attachment points\n";
    return GL_COLOR_ATTACHMENT0;
}

void OpenGL_FBO::createTexture(Attachment& attachment, GLenum internalFormat, GLenum format, GLenum dataType,
    GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT)
{
    attachment.texture.create2D(width, height, internalFormat, format, dataType);
    attachment.texture.setParameters(minFilter, magFilter, wrapS, wrapT);

    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment.attachmentPoint, GL_TEXTURE_2D,
        attachment.texture.getID(), 0);
}

OpenGL_FBO::Attachment::Attachment(Attachment&& other) noexcept :
    texture(std::move(other.texture)),
    type(other.type),
    attachmentPoint(other.attachmentPoint)
{
    other.type = AttachmentType::COLOR;
    other.attachmentPoint = -1;
}

OpenGL_FBO::Attachment& OpenGL_FBO::Attachment::operator=(Attachment&& other) noexcept
{
    if (this != &other)
    {
        texture = std::move(other.texture);

        type = other.type;
        attachmentPoint = other.attachmentPoint;

        other.type = AttachmentType::COLOR;
        other.attachmentPoint = -1;
    }
    return *this;
}
