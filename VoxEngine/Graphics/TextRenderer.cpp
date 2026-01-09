#include "TextRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Core/Profiler.h"
#include "Core/Assert.h"
#include "OpenGLWrappers/OpenGLDebug.h"

#include <iostream>

Glyph::Glyph(uint32_t textureID, const glm::ivec2& size, const glm::ivec2& bearing, GLuint advance) :
    textureID(textureID), size(size), bearing(bearing), advance(advance)
{
}

Glyph::~Glyph()
{
}

Glyph::Glyph(Glyph&& other) noexcept :
    textureID(other.textureID), size(other.size), bearing(other.bearing), advance(other.advance)
{
    other.textureID = 0;
}

Glyph& Glyph::operator=(Glyph&& other) noexcept
{
    if (this != &other)
    {
        textureID = other.textureID;
        size = other.size;
        bearing = other.bearing;
        advance = other.advance;

        other.textureID = 0;
    }
    return *this;
}

Font::~Font()
{
    if (textureArrayID)
    {
        glDeleteTextures(1, &textureArrayID);
        textureArrayID = 0;
    }
}

Font::Font(Font&& other) noexcept :
    glyphs(std::move(other.glyphs)), fontSize(other.fontSize), maxGlyphSize(other.maxGlyphSize), textureArrayID(other.textureArrayID)
{
    other.textureArrayID = 0;
}

Font& Font::operator=(Font&& other) noexcept
{
    if (this != &other)
    {
        glyphs = std::move(other.glyphs);
        fontSize = other.fontSize;
        maxGlyphSize = other.maxGlyphSize;
        textureArrayID = other.textureArrayID;

        other.textureArrayID = 0;
    }
    return *this;
}


TextRenderer& TextRenderer::getInstance()
{
	static TextRenderer textRenderer;
	return textRenderer;
}

uint32_t TextRenderer::decodeUTF8(const std::string& text, size_t& index)
{
    unsigned char c = text[index++];

    // 1-byte sequence (ASCII)
    if ((c & 0x80) == 0)
    {
        return c;
    }
    // 2-byte sequence
    else if ((c & 0xE0) == 0xC0)
    {
        uint32_t codepoint = (c & 0x1F) << 6;
        if (index < text.length())
            codepoint |= (text[index++] & 0x3F);
        return codepoint;
    }
    // 3-byte sequence
    else if ((c & 0xF0) == 0xE0)
    {
        uint32_t codepoint = (c & 0x0F) << 12;
        if (index < text.length())
            codepoint |= (text[index++] & 0x3F) << 6;
        if (index < text.length())
            codepoint |= (text[index++] & 0x3F);
        return codepoint;
    }
    // 4-byte sequence
    else if ((c & 0xF8) == 0xF0)
    {
        uint32_t codepoint = (c & 0x07) << 18;
        if (index < text.length())
            codepoint |= (text[index++] & 0x3F) << 12;
        if (index < text.length())
            codepoint |= (text[index++] & 0x3F) << 6;
        if (index < text.length())
            codepoint |= (text[index++] & 0x3F);
        return codepoint;
    }

    // Invalid UTF-8 sequence
    return 0;
}

TextRenderer::TextRenderer()
{
    // Shaders
	std::vector<Shader::ShaderSource> textShaderSources =
	{
		{GL_VERTEX_SHADER, "res/Shaders/text.vert"},
		{GL_FRAGMENT_SHADER, "res/Shaders/text.frag"}
	};
	textShader = Shader(textShaderSources);
    textShader.use();
    textShader.setInt("glyphTextureArray", 0);
    //textShader.setMat4("projection", projectionMatrix);

    // Buffers
    glm::vec2 vertices[4] = // CCW order
    {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f }
    };

    // Generate buffers
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glGenBuffers(1, &textInstanceVBO);

    // Bind VAO
    glBindVertexArray(textVAO);

    // VBO
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Vertex data
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);

    // Instance VBO
    glBindBuffer(GL_ARRAY_BUFFER, textInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, GLYPH_INSTANCE_BATCH_SIZE * sizeof(GlyphInstance), nullptr, GL_DYNAMIC_DRAW);

    // Instance data
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GlyphInstance), 0);
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GlyphInstance), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 1, GL_INT, sizeof(GlyphInstance), (void*)(6 * sizeof(float)));
    glVertexAttribDivisor(3, 1);
}

TextRenderer::~TextRenderer()
{
    fonts.clear();

    glDeleteBuffers(1, &textVBO);
    glDeleteVertexArrays(1, &textVAO);
}

void TextRenderer::flushGlyphs()
{
    if (glyphInstanceCount == 0)
    {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, textInstanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, glyphInstanceCount * sizeof(GlyphInstance), glyphInstances);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, glyphInstanceCount);

    glyphInstanceCount = 0;
}

void TextRenderer::pushGlyph(const GlyphInstance& glyph)
{
    ASSERT(glyphInstanceCount < GLYPH_INSTANCE_BATCH_SIZE);
    glyphInstances[glyphInstanceCount++] = glyph;
    if (glyphInstanceCount >= GLYPH_INSTANCE_BATCH_SIZE)
    {
        flushGlyphs();
    }
}

void TextRenderer::getFontInfo(FT_Face& face, glm::ivec2& maxGlyphSize, size_t& glyphCount)
{
    static_assert(sizeof(FT_ULong) == sizeof(uint32_t), "FT_Ulong != uint32_t");

    FT_UInt gindex;
    FT_ULong charcode = FT_Get_First_Char(face, &gindex);

    maxGlyphSize = { 0, 0 };
    glyphCount = 0;

    // TODO: Don't load charcodes <= 32 whatsoever. Add them to banned characters.
    while (gindex != 0)
    {
        if (FT_Load_Char(face, charcode, FT_LOAD_RENDER))
        {
            std::cerr << "[TextRenderer]: Failed to load glyph: '" << charcode << "'.\n";
            continue;
        }

        glm::ivec2 glyphSize = glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows);
        maxGlyphSize = glm::max(maxGlyphSize, glyphSize);

        charcode = FT_Get_Next_Char(face, charcode, &gindex);
        glyphCount++;
    }
}

void TextRenderer::loadGlyphs(FT_Face& face, Font& font)
{
    static_assert(sizeof(FT_ULong) == sizeof(uint32_t), "FT_Ulong != uint32_t");

    FT_UInt gindex;
    FT_ULong charcode = FT_Get_First_Char(face, &gindex);

    uint32_t textureID = 1;

    // TODO: Don't load charcodes <= 32 whatsoever. Add them to banned characters.
    while (gindex != 0)
    {
        if (FT_Load_Char(face, charcode, FT_LOAD_RENDER))
        {
            std::cerr << "[TextRenderer]: Failed to load glyph: '" << charcode << "'.\n";
            continue;
        }

        uint32_t texture = 0;
        if (charcode > ' ')
        {
            texture = textureID++;

            glTexSubImage3D
            (
                GL_TEXTURE_2D_ARRAY,
                0,
                0, 0, texture - 1,
                face->glyph->bitmap.width, face->glyph->bitmap.rows, 1,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );
        }

        Glyph glyph = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            (GLuint)face->glyph->advance.x
        };

        font.glyphs.emplace(charcode, std::move(glyph));

        charcode = FT_Get_Next_Char(face, charcode, &gindex);
    }
}

void TextRenderer::init()
{
	getInstance();
}

bool TextRenderer::loadFont(const std::string& fontName, GLuint fontSize)
{
    auto scopeName = std::string("Load font: ") + fontName;
    PROFILE_SCOPE(scopeName.c_str(), ProfileCategory::General);

	TextRenderer& inst = getInstance();
	auto& fonts = inst.fonts;

	// Test if font exists
	if (fonts.find(fontName) != fonts.end())
	{
		std::cerr << "[TextRenderer]: Font '" << fontName << "' already exists.\n";
		return false;
	}

    // Init FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cerr << "[TextRenderer]: Couldn't init FreeType Library.\n";
        return false;
    }

    std::string fontPath = "res/fonts/" + fontName + ".ttf";

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
    {
        std::cerr << "[TextRenderer]: Failed to load font: " << fontPath << ".\n";
        FT_Done_FreeType(ft);
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Create font
    Font font;
    font.fontSize = fontSize;

    // Get font info
    size_t glyphCount;
    getFontInfo(face, font.maxGlyphSize, glyphCount);

    // Create texture array
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &font.textureArrayID);
    glBindTexture(GL_TEXTURE_2D_ARRAY, font.textureArrayID);
    glTexImage3D(GL_TEXTURE_2D_ARRAY,
        0,
        GL_R8,
        font.maxGlyphSize.x, font.maxGlyphSize.y, glyphCount,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        nullptr
    );
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Load glyphs
    loadGlyphs(face, font);
    ASSERT(glyphCount == font.glyphs.size());

    // Done
    std::cout << "[TextRenderer]: Loaded font: '" << fontName << "' (" << fontPath << "). Character count: " << font.glyphs.size() << ". Max glyph size: (" << font.maxGlyphSize.x << ", " << font.maxGlyphSize.y << ").\n";

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    fonts.emplace(fontName, std::move(font));

    return true;
}

void TextRenderer::setCurrentFont(const std::string& fontName)
{
    TextRenderer& inst = getInstance();
    auto& fonts = inst.fonts;

    auto it = fonts.find(fontName);
    if (it == fonts.end())
    {
        std::cerr << "[TextRenderer]: Font '" << fontName << "' isn't loaded.\n";
        return;
    }

    inst.currentFont = &it->second;
}

void TextRenderer::renderText(const std::string& text, float x, float y, float rowHeight, const glm::vec3& color)
{
    PROFILE_SCOPE("Render: text", ProfileCategory::Render);

    TextRenderer& inst = getInstance();
    
    // Font
    const Font* font = inst.currentFont;
    if (!font)
    {
        std::cerr << "[TextRenderer]: Current font isn't set.\n";
        return;
    }

    const auto& glyphs = font->glyphs;
    const float scale = rowHeight / font->fontSize;
    const glm::vec2 invMaxGlyphSize = 1.0f / glm::vec2(font->maxGlyphSize);

    // Shader
    const auto& textShader = inst.textShader;
    textShader.use();
    textShader.setVec3("textColor", color.x, color.y, color.z);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glBindVertexArray(inst.textVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, font->textureArrayID);

    //
    const float startX = x;

    size_t index = 0;
    while (index < text.length())
    {
        uint32_t codepoint = decodeUTF8(text, index);

        if (codepoint == '\n')
        {
            y -= rowHeight;
            x = startX;
            continue;
        }

        auto it = glyphs.find(codepoint);
        if (it == glyphs.end())
        {
            codepoint = '?';
            it = glyphs.find(codepoint);
            if (it == glyphs.end())
            {
                continue;
            }
        }
        const Glyph& glyph = it->second;

        if (glyph.textureID)
        {
            float xpos = x + glyph.bearing.x * scale;
            float ypos = y - (glyph.size.y - glyph.bearing.y) * scale;
            float w = glyph.size.x * scale;
            float h = glyph.size.y * scale;

            GlyphInstance glyphInstance = {
                glm::vec4(xpos, ypos, w, h),
                glm::vec2(glyph.size.x * invMaxGlyphSize.x, glyph.size.y * invMaxGlyphSize.y),
                glyph.textureID - 1
            };

            inst.pushGlyph(glyphInstance);
        }

        x += (glyph.advance >> 6) * scale;
    }

    inst.flushGlyphs();
}

void TextRenderer::updateProjectionMatrix(int width, int height)
{
    TextRenderer& inst = getInstance();
    auto& proj = inst.projectionMatrix;
    const auto& textShader = inst.textShader;

    proj = glm::ortho(0.0f, (float)width, 0.0f, (float)height);;
    textShader.use();
    textShader.setMat4("projection", proj);
}
