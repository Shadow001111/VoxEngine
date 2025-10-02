#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>


Shader::Shader(const std::vector<ShaderSource>& sources)
{
    std::vector<GLuint> shaderIDs;
    for (const auto& src : sources)
    {
        std::string code = loadShaderSource(src.path);
        GLuint shader = compileShader(src.type, code);
        shaderIDs.push_back(shader);
    }

    ID = glCreateProgram();
    for (GLuint shader : shaderIDs)
        glAttachShader(ID, shader);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    for (GLuint shader : shaderIDs)
        glDeleteShader(shader);
}

Shader::~Shader()
{
    glDeleteProgram(ID);
}

void Shader::use() const
{
    glUseProgram(ID);
}

GLint Shader::getUniformLocation(const std::string& name) const
{
    auto it = uniformLocationCache.find(name);
    if (it != uniformLocationCache.end())
        return it->second;

    GLint location = glGetUniformLocation(ID, name.c_str());
    uniformLocationCache[name] = location;
    return location;
}

void Shader::setBool(const std::string& name, bool value) const
{
    glUniform1i(getUniformLocation(name), (int)value);
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(getUniformLocation(name), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(getUniformLocation(name), value);
}

void Shader::setFloatArray(const std::string& name, const float* values, size_t length) const
{
    glUniform1fv(getUniformLocation(name), length, values);
}

void Shader::setVec2(const std::string& name, float x, float y) const
{
	glUniform2f(getUniformLocation(name), x, y);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const
{
    glUniform3f(getUniformLocation(name), x, y, z);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setUvec2(const std::string& name, unsigned int x, unsigned int y) const
{
	glUniform2ui(getUniformLocation(name), x, y);
}

std::string Shader::loadShaderSource(const std::string& filePath) const
{
    std::ifstream file(filePath);
    std::stringstream buffer;
    if (file.is_open())
    {
        buffer << file.rdbuf();
        file.close();
    }
    else
    {
        std::cerr << "Error: failed to open shader file: " << filePath << std::endl;
    }
    return buffer.str();
}

GLuint Shader::compileShader(GLenum type, const std::string& source) const
{
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    std::string typeStr;
    switch (type) {
        case GL_VERTEX_SHADER: typeStr = "VERTEX"; break;
        case GL_FRAGMENT_SHADER: typeStr = "FRAGMENT"; break;
        case GL_COMPUTE_SHADER: typeStr = "COMPUTE"; break;
        case GL_GEOMETRY_SHADER: typeStr = "GEOMETRY"; break;
        case GL_TESS_CONTROL_SHADER: typeStr = "TESS_CONTROL"; break;
        case GL_TESS_EVALUATION_SHADER: typeStr = "TESS_EVALUATION"; break;
        default: typeStr = "UNKNOWN"; break;
    }
    checkCompileErrors(shader, typeStr);
    return shader;
}

void Shader::checkCompileErrors(GLuint shader, const std::string& type) const
{
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "Shader compilation error (" << type << "):\n" << infoLog << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "Program linking error:\n" << infoLog << std::endl;
        }
    }
}