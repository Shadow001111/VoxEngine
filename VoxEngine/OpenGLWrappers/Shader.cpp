#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

Shader::~Shader()
{
    if (id)
    {
        glDeleteProgram(id);
        id = 0;
    }
}

Shader::Shader(Shader&& other) noexcept
    : id(other.id),
    uniformLocationCache(std::move(other.uniformLocationCache))
{
    other.id = 0;
    other.uniformLocationCache.clear();
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        if (id)
        {
            glDeleteProgram(id);
        }

        id = other.id;
        uniformLocationCache = std::move(other.uniformLocationCache);

        other.id = 0;
        other.uniformLocationCache.clear();
    }
    return *this;
}

void Shader::create(const std::vector<ShaderSource>& sources)
{
    if (id)
    {
        glDeleteProgram(id);
        uniformLocationCache.clear();
    }

    std::vector<GLuint> shaderIDs;
    for (const auto& src : sources)
    {
        std::string code = loadShaderSource(src.path);
        GLuint shader = compileShader(src.type, code);
        shaderIDs.push_back(shader);
    }

    id = glCreateProgram();
    for (GLuint shader : shaderIDs)
        glAttachShader(id, shader);
    glLinkProgram(id);
    checkCompileErrors(id, "PROGRAM");

    for (GLuint shader : shaderIDs)
        glDeleteShader(shader);
}

void Shader::use() const
{
    glUseProgram(id);
}

GLint Shader::getUniformLocation(const std::string& name) const
{
    auto it = uniformLocationCache.find(name);
    if (it != uniformLocationCache.end())
        return it->second;

    GLint location = glGetUniformLocation(id, name.c_str());
    if (location == -1)
    {
        std::cerr << "[Shader][getUniformLocation]: Uniform '" << name << "' does not exist\n";
    }
    else
    {
        uniformLocationCache.emplace(name, location);
    }
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

void Shader::setIvec2(const std::string& name, int x, int y) const
{
	glUniform2i(getUniformLocation(name), x, y);
}

void Shader::setIvec3(const std::string& name, int x, int y, int z) const
{
	glUniform3i(getUniformLocation(name), x, y, z);
}

void Shader::setIvec4(const std::string& name, int x, int y, int z, int w) const
{
	glUniform4i(getUniformLocation(name), x, y, z, w);
}

void Shader::setUint(const std::string& name, unsigned int value) const
{
	glUniform1ui(getUniformLocation(name), value);
}

void Shader::setUvec2(const std::string& name, unsigned int x, unsigned int y) const
{
    glUniform2ui(getUniformLocation(name), x, y);
}

void Shader::setUvec3(const std::string& name, unsigned int x, unsigned int y, unsigned int z) const
{
	glUniform3ui(getUniformLocation(name), x, y, z);
}

void Shader::setUvec4(const std::string& name, unsigned int x, unsigned int y, unsigned int z, unsigned int w) const
{
	glUniform4ui(getUniformLocation(name), x, y, z, w);
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

void Shader::setVec4(const std::string& name, float x, float y, float z, float w) const
{
	glUniform4f(getUniformLocation(name), x, y, z, w);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
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
        std::cerr << "[Shader]: Failed to open shader file: '" << filePath << "'.\n";
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
            std::cerr << "[Shader]: Shader compilation error (" << type << "):\n" << infoLog<< "\n";
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "[Shader]: Program linking error:\n" << infoLog<< "\n";
        }
    }
}