#include "TextRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Core/Profiler.h"

#include <iostream>

Glyph::Glyph(uint32_t textureID, const glm::ivec2& size, const glm::ivec2& bearing, GLuint advance) :
    textureID(textureID), size(size), bearing(bearing), advance(advance)
{
}


Font::Font(Font&& other) noexcept :
    glyphs(std::move(other.glyphs)),
    fontSize(other.fontSize), 
    maxGlyphSize(other.maxGlyphSize),
    textureArray(std::move(other.textureArray))
{}

Font& Font::operator=(Font&& other) noexcept
{
    if (this != &other)
    {
        glyphs = std::move(other.glyphs);
        fontSize = other.fontSize;
        maxGlyphSize = other.maxGlyphSize;
        textureArray = std::move(other.textureArray);
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
	textShader.create(textShaderSources);
    //textShader.setInt("glyphTextureArray", 0);
    //textShader.setMat4("projection", projectionMatrix);

    // Buffers
    glm::vec2 vertices[4] = // CCW order
    {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f }
    };

    // Create VAO
    textVAO.create();

    // Create VBO
    textVBO.create(GL_ARRAY_BUFFER);
    textVBO.allocateStorage(sizeof(vertices), 0, vertices);

    // Bind VBO to VAO
    textVAO.bindVertexBuffer(0, textVBO.getID(), 0, sizeof(vertices[0]));

    textVAO.enableAttribute(0);
    textVAO.setFloatAttribute(0, 2, 0, 0);

    // Create instance VBO
    textInstanceVBO.create(GL_ARRAY_BUFFER);
    textInstanceVBO.allocateStorage(sizeof(GlyphInstance) * GLYPH_INSTANCE_BATCH_SIZE, GL_DYNAMIC_STORAGE_BIT);

    // Bind instance VBO to VAO
    textVAO.bindVertexBuffer(1, textInstanceVBO.getID(), 0, sizeof(GlyphInstance));

    textVAO.enableAttribute(1);
    textVAO.setFloatAttribute(1, 4, 0, 1);
    textVAO.setAttributeDivisor(1, 1);

    textVAO.enableAttribute(2);
    textVAO.setFloatAttribute(2, 2, 4 * sizeof(float), 1);
    textVAO.setAttributeDivisor(2, 1);

    textVAO.enableAttribute(3);
    textVAO.setIntAttribute(3, 1, 6 * sizeof(float), 1);
    textVAO.setAttributeDivisor(3, 1);
}

void TextRenderer::flushGlyphs()
{
    if (glyphInstanceCount == 0)
    {
        return;
    }

    // Move glyph data to GPU
    textInstanceVBO.write(glyphInstances, glyphInstanceCount * sizeof(GlyphInstance));

    // Draw glyphs
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, glyphInstanceCount);

    // Reset glyphs count
    glyphInstanceCount = 0;
}

void TextRenderer::pushGlyph(const GlyphInstance& glyph)
{
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

            font.textureArray.uploadSubData2DArray(
                face->glyph->bitmap.buffer,
                0, 0,
                texture - 1,
                face->glyph->bitmap.width, face->glyph->bitmap.rows,
                GL_UNSIGNED_BYTE
            );
        }

        Glyph glyph =
        {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            (GLuint)face->glyph->advance.x
        };

        font.glyphs.emplace(charcode, glyph);

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
		std::cerr << "[TextRenderer][loadFont]: Font '" << fontName << "' already exists\n";
		return false;
	}

    // Init FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cerr << "[TextRenderer][loadFont]: Couldn't init FreeType Library\n";
        return false;
    }

    std::string fontPath = "res/fonts/" + fontName + ".ttf";

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
    {
        std::cerr << "[TextRenderer][loadFont]: Failed to load font: " << fontPath << "\n";
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
    GLint oldAlignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldAlignment);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    font.textureArray.create2DArray(font.maxGlyphSize.x, font.maxGlyphSize.y, glyphCount, GL_R8);

    font.textureArray.setParameters(GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    // Load glyphs
    loadGlyphs(face, font);

    // Done
    std::cout
        << "[TextRenderer][loadFont]: Loaded font: '" << fontName << "' (" << fontPath << "). Character count: " << font.glyphs.size()
        << ". Max glyph size: (" << font.maxGlyphSize.x << ", " << font.maxGlyphSize.y << ").\n";

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glPixelStorei(GL_UNPACK_ALIGNMENT, oldAlignment);

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

void TextRenderer::startTextRendering()
{
    TextRenderer& inst = getInstance();

    const Font* font = inst.currentFont;
    if (!font)
    {
        std::cerr << "[TextRenderer][renderText]: Current font is not set\n";
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    inst.textVAO.bind();
    font->textureArray.bindUnit(0);

    const auto& textShader = inst.textShader;
    textShader.use();
}

void TextRenderer::renderText(const std::string& text, float x, float y, float rowHeight, const glm::vec3& color)
{
    PROFILE_SCOPE("Render text", ProfileCategory::Render);

    TextRenderer& inst = getInstance();
    
    // Font
    const Font* font = inst.currentFont;
    if (!font)
    {
        std::cerr << "[TextRenderer][renderText]: Current font is not set\n";
        return;
    }

    const auto& glyphs = font->glyphs;
    const float scale = rowHeight / font->fontSize;
    const glm::vec2 invMaxGlyphSize = 1.0f / glm::vec2(font->maxGlyphSize);

    // Shader
    const auto& textShader = inst.textShader;
    textShader.setVec3("textColor", color.x, color.y, color.z);

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

    proj = glm::ortho(0.0f, (float)width, 0.0f, (float)height);
    textShader.setMat4("projection", proj);
}
