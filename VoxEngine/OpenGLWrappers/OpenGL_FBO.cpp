#include "OpenGL_FBO.h"

#include <iostream>

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

void OpenGL_FBO::create(int width, int height)
{
    // Clean up
    if (id)
    {
        for (auto& pair : attachments)
        {
            auto& attachment = pair.second;
            // Detach from framebuffer
            if (attachment.attachmentPoint != -1)
            {
                glNamedFramebufferTexture(id, attachment.attachmentPoint, 0, 0);
            }
        }
        attachments.clear();

        glDeleteFramebuffers(1, &id);

        drawBuffers.clear();
    }

    // Create new frame buffer
    glCreateFramebuffers(1, &id);

    // Set new resolution
    this->width = width;
    this->height = height;
}

void OpenGL_FBO::createColorAttachment(const std::string& name, GLenum internalFormat,
    GLenum format, GLenum dataType,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::COLOR;
    attachment.attachmentPoint = getAttachmentPoint(AttachmentType::COLOR);

    createAndAttachTexture(attachment, internalFormat, format, dataType, minFilter, magFilter, wrapS, wrapT);
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

    createAndAttachTexture(attachment, internalFormat, format, dataType, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);
}

void OpenGL_FBO::createStencilAttachment(const std::string& name, GLenum internalFormat,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::STENCIL;
    attachment.attachmentPoint = GL_STENCIL_ATTACHMENT;

    createAndAttachTexture(attachment, internalFormat, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
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

    createAndAttachTexture(attachment, internalFormat, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
        minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);
}

void OpenGL_FBO::createStandaloneTextureAttachment(const std::string& name,
    GLenum internalFormat, GLenum format, GLenum dataType,
    float resolutionFactor,
    GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::STANDALONE_TEXTURE;
    attachment.attachmentPoint = -1;
    attachment.resolutionFactor = resolutionFactor;

    createStandaloneTexture(attachment, internalFormat, format, dataType, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);
}

void OpenGL_FBO::removeAttachment(const std::string& name)
{
    auto it = attachments.find(name);
    if (it != attachments.end())
    {
        // Detach from framebuffer
        if (it->second.attachmentPoint != -1)
        {
            glNamedFramebufferTexture(id, it->second.attachmentPoint, 0, 0);
        }
        attachments.erase(it);
    }
}

void OpenGL_FBO::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, id);
}

void OpenGL_FBO::bind(GLenum target) const
{
    glBindFramebuffer(target, id);
}

void OpenGL_FBO::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGL_FBO::unbind(GLenum target)
{
    glBindFramebuffer(target, 0);
}

void OpenGL_FBO::setDrawBuffers(const std::vector<std::string>& attachmentNames)
{
    if (attachmentNames.empty())
    {
        // If no attachments specified, disable drawing
        glNamedFramebufferDrawBuffer(id, GL_NONE);
        glNamedFramebufferReadBuffer(id, GL_NONE);
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
        glNamedFramebufferDrawBuffer(id, GL_NONE);
        glNamedFramebufferReadBuffer(id, GL_NONE);
    }
    else
    {
        glNamedFramebufferDrawBuffers(id, static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    }
}

void OpenGL_FBO::resize(int newWidth, int newHeight)
{
    if (newWidth == width && newHeight == height)
    {
        return;
    }
    if (newWidth <= 0 || newHeight <= 0)
    {
        std::cerr << "[OpenGL_FBO][resize]: Invalid franebuffer dimensions: width=" << newWidth << ", height=" << newHeight << "\n";
        return;
    }

    width = newWidth;
    height = newHeight;

    for (auto& [name, attachment] : attachments)
    {
        if (attachment.type == AttachmentType::STANDALONE_TEXTURE)
        {
            int tWidth = static_cast<int>(width * attachment.resolutionFactor);
            int tHeight = static_cast<int>(height * attachment.resolutionFactor);
            attachment.texture.recreate2D(tWidth, tHeight);
        }
        else
        {
            attachment.texture.recreate2D(width, height);
            glNamedFramebufferTexture(id, attachment.attachmentPoint, attachment.texture.getID(), 0);
        }
    }
}

void OpenGL_FBO::blitTo(const OpenGL_FBO& dstFBO, GLbitfield mask, GLenum filter) const
{
    glBlitNamedFramebuffer(
        id, dstFBO.id,
        0, 0, width, height,
        0, 0, dstFBO.width, dstFBO.height,
        mask, filter);
}

void OpenGL_FBO::blitToDefaultFramebuffer(int dstWidth, int dstHeight, GLbitfield mask, GLenum filter) const
{
    glBlitNamedFramebuffer(
        id, 0,
        0, 0, width, height,
        0, 0, dstWidth, dstHeight,
        mask, filter);
}

void OpenGL_FBO::clearAttachment(const std::string& name, const float* clearValue) const
{
    auto it = attachments.find(name);
    if (it == attachments.end())
    {
        std::cerr << "[OpenGL_FBO][clearAttachment]: Attachment '" << name << "' not found in attachments for clearing\n";
        return;
    }

    const auto& attachment = it->second;
    const auto& texture = attachment.texture;

    switch (attachment.type)
    {
    case AttachmentType::COLOR:
    case AttachmentType::STANDALONE_TEXTURE:
        glClearTexImage(texture.getID(), 0, texture.getFormat(), GL_FLOAT, clearValue);
        break;
    case AttachmentType::DEPTH:
        glClearNamedFramebufferfv(id, GL_DEPTH, 0, clearValue);
        break;
    case AttachmentType::STENCIL:
    {
        GLint stencilValue = static_cast<GLint>(clearValue[0]);
        glClearNamedFramebufferiv(id, GL_STENCIL, 0, &stencilValue);
        break;
    }
    case AttachmentType::DEPTH_STENCIL:
        glClearNamedFramebufferfi(id, GL_DEPTH_STENCIL, 0, clearValue[0], static_cast<GLint>(clearValue[1]));
        break;
    }
}

void OpenGL_FBO::clearDrawBuffer(const std::string& name, const float* clearValue) const
{
    bool found = false;
    int index = 0;

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
        glClearNamedFramebufferfv(id, GL_COLOR, index, clearValue);
    }
    else
    {
        std::cerr << "[OpenGL_FBO][setDrawBuffers]: Attachment '" << name << "' not found in draw buffers for clearing\n";
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

void OpenGL_FBO::bindTextureToUnit(const std::string& name, GLuint textureUnit) const
{
    auto it = attachments.find(name);
    if (it != attachments.end())
    {
        it->second.texture.bindUnit(textureUnit);
    }
    else
    {
        std::cerr << "[OpengGL_FBO][bindTextureToUnit]: Texture/attachment '" << name << "' is not found\n";
    }
}

bool OpenGL_FBO::isComplete() const
{
    GLenum status = glCheckNamedFramebufferStatus(id, GL_FRAMEBUFFER);
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
    std::cerr << "[OpenGL_FBO][isComplete]: Framebuffer is not complete: " << errorMsg << "\n";
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
        default: return -1;
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

void OpenGL_FBO::createAndAttachTexture(
    Attachment& attachment, GLenum internalFormat, GLenum format, GLenum dataType,
    GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT)
{
    attachment.texture.create2D(width, height, internalFormat, format, dataType);
    attachment.texture.setParameters(minFilter, magFilter, wrapS, wrapT);

    if (attachment.attachmentPoint != -1)
    {
        glNamedFramebufferTexture(id, attachment.attachmentPoint, attachment.texture.getID(), 0);
    }
}

void OpenGL_FBO::createStandaloneTexture(
    Attachment& attachment, GLenum internalFormat, GLenum format, GLenum dataType,
    GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT)
{
    int tWidth = static_cast<int>(width * attachment.resolutionFactor);
    int tHeight = static_cast<int>(height * attachment.resolutionFactor);
    attachment.texture.create2D(tWidth, tHeight, internalFormat, format, dataType);
    attachment.texture.setParameters(minFilter, magFilter, wrapS, wrapT);
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
