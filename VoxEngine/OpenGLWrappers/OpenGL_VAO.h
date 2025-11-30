#pragma once
#include <glad/glad.h>
#include <vector>

class OpenGL_VAO
{
public:
    enum class Type : uint8_t
    {
        Float,
        Int
    };

    OpenGL_VAO();
    ~OpenGL_VAO();

    // Delete copy constructor/assignment
    OpenGL_VAO(const OpenGL_VAO& other) = delete;
    OpenGL_VAO& operator=(const OpenGL_VAO& other) = delete;

    // Move constructor/assignment
    OpenGL_VAO(OpenGL_VAO&& other) noexcept;
    OpenGL_VAO& operator=(OpenGL_VAO&& other) noexcept;

    void swap(OpenGL_VAO& other) noexcept;

    // VAO operations
    void bind() const;
    void unbind() const;
    static void unbindGlobal();

    // Attribute management
    void setVertexSize(GLsizei vertexSize);
    void setAttribute(Type type, GLint componentsCount, bool normalized);
    void enableAttribute(GLuint index);
    void disableAttribute(GLuint index);
    void setAttributeDivisor(GLuint index, GLuint divisor);

    // Getters
    GLuint getID() const { return id; }
private:
    GLuint id = 0;
    GLsizei vertexSize = 0;
    GLuint attributeCount = 0;
    GLsizei attributeOffset = 0;
};