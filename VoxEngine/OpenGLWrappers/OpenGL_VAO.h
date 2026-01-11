#pragma once
#include <glad/glad.h>

class OpenGL_VAO
{
protected:
    GLuint id = 0;
public:
    OpenGL_VAO() = default;
    ~OpenGL_VAO();

    OpenGL_VAO(const OpenGL_VAO& other) = delete;
    OpenGL_VAO& operator=(const OpenGL_VAO& other) = delete;

    OpenGL_VAO(OpenGL_VAO&& other) noexcept;
    OpenGL_VAO& operator=(OpenGL_VAO&& other) noexcept;

    void create();

    void bind() const;
    static void unbind();

    void setFloatAttribute(
        GLuint attributeIndex, GLint componentCount, GLsizei stride, GLuint bindingIndex = 0,
        GLenum type = GL_FLOAT, GLboolean normalized = GL_FALSE);

    void setIntAttribute(
        GLuint attributeIndex, GLint componentCount, GLsizei stride, GLuint bindingIndex = 0,
        GLenum type = GL_INT);

    void enableAttribute(GLuint index);
    void disableAttribute(GLuint index);
    void setAttributeDivisor(GLuint index, GLuint divisor);

    void bindVertexBuffer(GLuint bindingIndex, GLuint bufferId, GLintptr offset, GLsizei stride);
    void bindElementBuffer(GLuint bufferId);

    GLuint getID() const { return id; };
};