#pragma once
#include <glm/glm.hpp>
#include <memory>

#include "Shader.h"

#include <ft2build.h>
#include FT_FREETYPE_H

struct Character
{
    GLuint textureID = 0;   // ID of glyph texture
    glm::ivec2 size;    // Glyph size
    glm::ivec2 bearing; // Offset from baseline
    GLuint advance;     // Horizontal offset to next glyph

    Character() = default;
    Character(GLuint textureID, const glm::ivec2& size, const glm::ivec2& bearing, GLuint advance);
    ~Character();

    Character(const Character& other) = delete;
    Character& operator=(const Character& other) = delete;

    Character(Character&& other) noexcept;
    Character& operator=(Character&& other) noexcept;
};

struct Font
{
    std::unordered_map<uint32_t, Character> characters;
    float fontSize;

    Font() = default;
    ~Font() = default;

    Font(const Font& other) = delete;
    Font& operator=(const Font& other) = delete;

    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;
};

class TextRenderer
{
    std::unordered_map<std::string, Font> fonts;
    Font* currentFont = nullptr;

    GLuint textVAO, textVBO;
    std::unique_ptr<Shader> textShader;
    glm::mat4 projectionMatrix;

    TextRenderer();
    ~TextRenderer();
    TextRenderer(const TextRenderer& other) = delete;
    TextRenderer& operator=(const TextRenderer& other) = delete;
    TextRenderer(TextRenderer&& other) = delete;
    TextRenderer& operator=(TextRenderer&& other) = delete;

    static TextRenderer& getInstance();

    static uint32_t decodeUTF8(const std::string& text, size_t& index);
public:
    static void init();

    static bool loadFont(const std::string& fontName, GLuint fontSize);

    static void setCurrentFont(const std::string& fontName);

    static void renderText(const std::string& text, float x, float y, float rowHeight, const glm::vec3& color);

    static void updateProjectionMatrix(int width, int height);
};