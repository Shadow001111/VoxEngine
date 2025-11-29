#pragma once
#include <glm/glm.hpp>

#include "Shader.h"

#include <ft2build.h>
#include FT_FREETYPE_H

struct Glyph
{
    uint32_t textureID = 0;
    glm::ivec2 size;    // Glyph size
    glm::ivec2 bearing; // Offset from baseline
    GLuint advance;     // Horizontal offset to next glyph

    Glyph() = default;
    Glyph(uint32_t textureID, const glm::ivec2& size, const glm::ivec2& bearing, GLuint advance);
    ~Glyph();

    Glyph(const Glyph& other) = delete;
    Glyph& operator=(const Glyph& other) = delete;

    Glyph(Glyph&& other) noexcept;
    Glyph& operator=(Glyph&& other) noexcept;
};

struct Font
{
    std::unordered_map<uint32_t, Glyph> glyphs;
    float fontSize = 0.0f;
    glm::ivec2 maxGlyphSize = { 0, 0 };
    GLuint textureArrayID = 0;

    Font() = default;
    ~Font();

    Font(const Font& other) = delete;
    Font& operator=(const Font& other) = delete;

    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;
};

struct GlyphInstance
{
    glm::vec4 positionAndSize;
    glm::vec2 texSize;
    uint32_t textureID;
};

constexpr size_t GLYPH_INSTANCE_BATCH_SIZE = 1024;

class TextRenderer
{
    std::unordered_map<std::string, Font> fonts;
    Font* currentFont = nullptr;

    GLuint textVAO = 0, textVBO = 0, textInstanceVBO = 0;

    Shader textShader;
    glm::mat4 projectionMatrix;

    GlyphInstance glyphInstances[GLYPH_INSTANCE_BATCH_SIZE];
    size_t glyphInstanceCount = 0;

    TextRenderer();
    ~TextRenderer();
    TextRenderer(const TextRenderer& other) = delete;
    TextRenderer& operator=(const TextRenderer& other) = delete;
    TextRenderer(TextRenderer&& other) = delete;
    TextRenderer& operator=(TextRenderer&& other) = delete;

    static TextRenderer& getInstance();

    static uint32_t decodeUTF8(const std::string& text, size_t& index);

    void flushGlyphs();
    void pushGlyph(const GlyphInstance& glyph);

    static void getFontInfo(FT_Face& face, glm::ivec2& maxGlyphSize, size_t& glyphCount);
    static void loadGlyphs(FT_Face& face, Font& font);
public:
    static void init();

    static bool loadFont(const std::string& fontName, GLuint fontSize);

    static void setCurrentFont(const std::string& fontName);

    static void renderText(const std::string& text, float x, float y, float rowHeight, const glm::vec3& color);

    static void updateProjectionMatrix(int width, int height);
};