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
    // Textures are automatically managed by OpenGL_Texture destructor
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
    attachment.isExternal = false;
    attachment.attachmentPoint = getAttachmentPoint(AttachmentType::COLOR);

    createTexture(attachment, internalFormat, format, dataType, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);

    setupDrawBuffers();

    if (!isComplete())
    {
        std::cerr << "Framebuffer is not complete after adding color attachment: " << name << std::endl;
    }
}

void OpenGL_FBO::createDepthAttachment(const std::string& name, GLenum internalFormat,
    GLenum format, GLenum dataType,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::DEPTH;
    attachment.isExternal = false;
    attachment.attachmentPoint = GL_DEPTH_ATTACHMENT;

    createTexture(attachment, internalFormat, format, dataType, minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);

    if (!isComplete())
    {
        std::cerr << "Framebuffer is not complete after adding depth attachment: " << name << std::endl;
    }
}

void OpenGL_FBO::createStencilAttachment(const std::string& name, GLenum internalFormat,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::STENCIL;
    attachment.isExternal = false;
    attachment.attachmentPoint = GL_STENCIL_ATTACHMENT;

    createTexture(attachment, internalFormat, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
        minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);

    if (!isComplete())
    {
        std::cerr << "Framebuffer is not complete after adding stencil attachment: " << name << std::endl;
    }
}

void OpenGL_FBO::createDepthStencilAttachment(const std::string& name, GLenum internalFormat,
    GLenum minFilter, GLenum magFilter,
    GLenum wrapS, GLenum wrapT)
{
    Attachment attachment;
    attachment.type = AttachmentType::DEPTH_STENCIL;
    attachment.isExternal = false;
    attachment.attachmentPoint = GL_DEPTH_STENCIL_ATTACHMENT;

    createTexture(attachment, internalFormat, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
        minFilter, magFilter, wrapS, wrapT);
    attachments[name] = std::move(attachment);

    if (!isComplete())
    {
        std::cerr << "Framebuffer is not complete after adding depth-stencil attachment: " << name << std::endl;
    }
}

void OpenGL_FBO::linkColorTexture(const std::string& name, OpenGL_Texture& texture, int attachmentPoint)
{
    Attachment attachment;
    attachment.texture = std::move(texture);
    attachment.type = AttachmentType::COLOR;
    attachment.isExternal = true;
    attachment.attachmentPoint = getAttachmentPoint(AttachmentType::COLOR, attachmentPoint);

    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment.attachmentPoint, GL_TEXTURE_2D,
        attachment.texture.getID(), 0);
    attachments[name] = std::move(attachment);

    setupDrawBuffers();

    if (!isComplete())
    {
        std::cerr << "Framebuffer is not complete after linking color texture: " << name << std::endl;
    }
}

void OpenGL_FBO::linkDepthTexture(const std::string& name, OpenGL_Texture& texture)
{
    Attachment attachment;
    attachment.texture = std::move(texture);
    attachment.type = AttachmentType::DEPTH;
    attachment.isExternal = true;
    attachment.attachmentPoint = GL_DEPTH_ATTACHMENT;

    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
        attachment.texture.getID(), 0);
    attachments[name] = std::move(attachment);

    if (!isComplete())
    {
        std::cerr << "Framebuffer is not complete after linking depth texture: " << name << std::endl;
    }
}

void OpenGL_FBO::linkStencilTexture(const std::string& name, OpenGL_Texture& texture)
{
    Attachment attachment;
    attachment.texture = std::move(texture);
    attachment.type = AttachmentType::STENCIL;
    attachment.isExternal = true;
    attachment.attachmentPoint = GL_STENCIL_ATTACHMENT;

    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
        attachment.texture.getID(), 0);
    attachments[name] = std::move(attachment);

    if (!isComplete())
    {
        std::cerr << "Framebuffer is not complete after linking stencil texture: " << name << std::endl;
    }
}

void OpenGL_FBO::linkDepthStencilTexture(const std::string& name, OpenGL_Texture& texture)
{
    Attachment attachment;
    attachment.texture = std::move(texture);
    attachment.type = AttachmentType::DEPTH_STENCIL;
    attachment.isExternal = true;
    attachment.attachmentPoint = GL_DEPTH_STENCIL_ATTACHMENT;

    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
        attachment.texture.getID(), 0);
    attachments[name] = std::move(attachment);

    if (!isComplete())
    {
        std::cerr << "Framebuffer is not complete after linking depth-stencil texture: " << name << std::endl;
    }
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

    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

    for (auto& [name, attachment] : attachments)
    {
        if (!attachment.isExternal)
        {
            // Get format information from the existing texture
            GLenum internalFormat = attachment.texture.getInternalFormat();
            GLenum format = attachment.texture.getFormat();
            GLenum dataType = attachment.texture.getDataType();

            // Recreate the texture with new size
            attachment.texture.create2D(width, height, internalFormat, format, dataType);

            // Reattach to framebuffer
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment.attachmentPoint, GL_TEXTURE_2D,
                attachment.texture.getID(), 0);
        }
    }
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
        OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

        switch (it->second.type)
        {
        case AttachmentType::COLOR:
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

OpenGL_Texture& OpenGL_FBO::getTexture(const std::string& name)
{
    auto it = attachments.find(name);
    if (it != attachments.end())
        return it->second.texture;
    throw std::runtime_error("Texture not found: " + name);
}

const OpenGL_Texture& OpenGL_FBO::getTexture(const std::string& name) const
{
    auto it = attachments.find(name);
    if (it != attachments.end())
        return it->second.texture;
    throw std::runtime_error("Texture not found: " + name);
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
    std::cerr << "[OpenGL][Framebuffer]: Framebuffer is not complete: " << errorMsg << std::endl;
    return false;
}

void OpenGL_FBO::setupDrawBuffers()
{
    OPENGL_CHECK_BIND_TARGET(id, GL_FRAMEBUFFER);

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

void OpenGL_FBO::createTexture(Attachment& attachment, GLenum internalFormat, GLenum format, GLenum dataType,
    GLenum minFilter, GLenum magFilter, GLenum wrapS, GLenum wrapT)
{
    attachment.texture.create2D(width, height, internalFormat, format, dataType);
    attachment.texture.setParameters(minFilter, magFilter, wrapS, wrapT);

    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment.attachmentPoint, GL_TEXTURE_2D,
        attachment.texture.getID(), 0);
}

OpenGL_FBO::Attachment::Attachment(Attachment&& other) noexcept :
    texture(std::move(other.texture))
    , type(other.type)
    , attachmentPoint(other.attachmentPoint)
    , isExternal(other.isExternal)
{
}

OpenGL_FBO::Attachment& OpenGL_FBO::Attachment::operator=(Attachment&& other) noexcept
{
    if (this != &other)
    {
        texture = std::move(other.texture);
        type = other.type;
        attachmentPoint = other.attachmentPoint;
        isExternal = other.isExternal;
    }
    return *this;
}
