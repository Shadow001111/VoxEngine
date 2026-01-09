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
    void unbind() const;

    void setFloatAttribute(GLuint index, GLint componentCount, GLenum type,
        GLboolean normalized, GLsizei stride, const void* pointer);

    void setIntAttribute(GLuint index, GLint componentCount, GLenum type,
        GLsizei stride, const void* pointer);

    void enableAttribute(GLuint index);
    void disableAttribute(GLuint index);

    void setAttributeDivisor(GLuint index, GLuint divisor);

    GLuint getID() const { return id; };
};