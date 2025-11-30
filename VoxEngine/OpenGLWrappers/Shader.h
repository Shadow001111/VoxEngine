#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/mat4x4.hpp>

class Shader
{
    GLuint ID = 0;
    bool initialized = false;
    mutable std::unordered_map<std::string, GLint> uniformLocationCache;
public:
    struct ShaderSource
    {
        GLenum type;
        std::string path;
    };

    Shader() = default;
    Shader(const std::vector<ShaderSource>& sources);
    ~Shader();

    Shader(const Shader& other) = delete;
    Shader& operator=(const Shader& other) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void init(const std::vector<ShaderSource>& sources);

    void use() const;

    void setBool(const std::string& name, bool value) const;

    void setInt(const std::string& name, int value) const;
	void setIvec2(const std::string& name, int x, int y) const;
	void setIvec3(const std::string& name, int x, int y, int z) const;
	void setIvec4(const std::string& name, int x, int y, int z, int w) const;

	void setUint(const std::string& name, unsigned int value) const;
    void setUvec2(const std::string& name, unsigned int x, unsigned int y) const;
	void setUvec3(const std::string& name, unsigned int x, unsigned int y, unsigned int z) const;
	void setUvec4(const std::string& name, unsigned int x, unsigned int y, unsigned int z, unsigned int w) const;

    void setFloat(const std::string& name, float value) const;
    void setFloatArray(const std::string& name, const float* values, size_t length) const;
	void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
	void setVec4(const std::string& name, float x, float y, float z, float w) const;

    void setMat4(const std::string& name, const glm::mat4& mat) const;

    GLuint getID() const { return ID; }
    bool isInitialized() const { return initialized; }
private:
    GLint getUniformLocation(const std::string& name) const;

    std::string loadShaderSource(const std::string& filePath) const;
    
    GLuint compileShader(GLenum type, const std::string& source) const;

    void checkCompileErrors(GLuint shader, const std::string& type) const;
};