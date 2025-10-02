#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/mat4x4.hpp>

class Shader
{
public:
    struct ShaderSource
    {
        GLenum type;
        std::string path;
    };

    Shader(const std::vector<ShaderSource>& sources);
    ~Shader();
    Shader(const Shader& other) = delete;
    Shader operator=(const Shader& other) = delete;

    void use() const;

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setFloatArray(const std::string& name, const float* values, size_t length) const;
	void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;
	void setUvec2(const std::string& name, unsigned int x, unsigned int y) const;

    GLuint getID() const { return ID; }

private:
    GLuint ID;
    mutable std::unordered_map<std::string, GLint> uniformLocationCache;

    GLint getUniformLocation(const std::string& name) const;

    std::string loadShaderSource(const std::string& filePath) const;
    
    GLuint compileShader(GLenum type, const std::string& source) const;

    void checkCompileErrors(GLuint shader, const std::string& type) const;
};