#include "TextRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Core/Profiler.h"
#include "Core/Decoding/UTFDecoder.h"

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
{
}

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

uint32_t TextRenderer::decodeStdString(const void* byteBuffer, size_t bufferLength, size_t& index)
{
    if (index >= bufferLength) return UTFDecoder::INVALID_CODEPOINT;
    const uint8_t* text_ = static_cast<const uint8_t*>(byteBuffer);
    return text_[index++];
}

uint32_t TextRenderer::decodeUTF8(const void* byteBuffer, size_t bufferLength, size_t& index)
{
    return UTFDecoder::decodeUTF8CodePoint(static_cast<const char8_t*>(byteBuffer), bufferLength, index);
}

uint32_t TextRenderer::decodeUTF16(const void* byteBuffer, size_t bufferLength, size_t& index)
{
    return UTFDecoder::decodeUTF16CodePoint(static_cast<const char16_t*>(byteBuffer), bufferLength, index);
}

uint32_t TextRenderer::decodeUTF32(const void* byteBuffer, size_t bufferLength, size_t& index)
{
    return UTFDecoder::decodeUTF32CodePoint(static_cast<const char32_t*>(byteBuffer), bufferLength, index);
}

TextRenderer::TextRenderer() :
    projectionMatrix(glm::identity<glm::mat4>())
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
}

void TextRenderer::flushGlyphs()
{
    if (glyphInstanceCount == 0)
    {
        return;
    }

    // Move glyph data to GPU
    textInstanceVBO.write(glyphInstances.get(), glyphInstanceCount * sizeof(GlyphInstance));

    // Draw glyphs
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, glyphInstanceCount);

    // Reset glyphs count
    glyphInstanceCount = 0;
}

void TextRenderer::pushGlyph(const GlyphInstance& glyph)
{
    glyphInstances[glyphInstanceCount++] = glyph;
    if (glyphInstanceCount >= glyphInstanceBatchSize)
    {
        flushGlyphs();
    }
}

void TextRenderer::createInstanceVBO(size_t glyphCount)
{
    // Create instance VBO
    textInstanceVBO.create(GL_ARRAY_BUFFER);
    textInstanceVBO.allocateStorage(sizeof(GlyphInstance) * glyphCount, GL_DYNAMIC_STORAGE_BIT);

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

void TextRenderer::renderTextInternal(const void* text, size_t textLength, UTFDecoderFunction decoder, float x, float y, float rowHeight,
    const glm::vec3& color, TextAlignment alignment, const glm::vec2& bounds)
{
    PROFILE_SCOPE("Render text", ProfileCategory::Render);

    TextRenderer& inst = getInstance();

    // Check buffer
    if (inst.glyphInstanceBatchSize == 0)
    {
        std::cerr << "[TextRenderer][renderText]: Glyph instance batch size is zero\n";
        return;
    }

    // Font
    const Font* font = inst.currentFont;
    if (!font)
    {
        std::cerr << "[TextRenderer][renderText]: Current font is not set\n";
        return;
    }

    //
    const auto& glyphs = font->glyphs;
    float scale = rowHeight / font->fontSize;
    const glm::vec2 invMaxGlyphSize = 1.0f / glm::vec2(font->maxGlyphSize);

    // Shader
    const auto& textShader = inst.textShader;
    textShader.setVec3("textColor", color.x, color.y, color.z);

    // Decode text
    std::vector<uint32_t> codepoints;
    codepoints.reserve(textLength);

    size_t index = 0;
    float totalWidth = 0.0f;
    float maxLineWidth = 0.0f;
    int lineCount = 1;
    float currentLineWidth = 0.0f;

    const uint32_t replaceCodepoint = '?';
    const bool doesNotReplaceCodepointExist = glyphs.find(replaceCodepoint) == glyphs.end();

    while (index < textLength)
    {
        uint32_t codepoint = decoder(text, textLength, index);

        if (codepoint == UTFDecoder::INVALID_CODEPOINT)
        {
            break;
        }
        else if (codepoint == '\n')
        {
            // Update max line width for current line
            maxLineWidth = std::max(maxLineWidth, currentLineWidth);
            currentLineWidth = 0.0f;
            lineCount++;
            codepoints.push_back(codepoint);
            continue;
        }

        // Check if glyph exists, replace if not
        auto it = glyphs.find(codepoint);
        if (it == glyphs.end())
        {
            if (doesNotReplaceCodepointExist)
            {
                continue;
            }
            codepoint = replaceCodepoint;
        }

        const Glyph& glyph = it->second;
        currentLineWidth += (glyph.advance >> 6) * scale;

        codepoints.push_back(codepoint);
    }

    // Update max line width for the last line
    maxLineWidth = std::max(maxLineWidth, currentLineWidth);

    // Calculate text bounds
    float textWidth = maxLineWidth;
    float textHeight = lineCount * rowHeight;

    // Apply bounds scaling if needed
    float widthScale = 1.0f;
    float heightScale = 1.0f;

    if (bounds.x > 0.0f && textWidth > bounds.x)
    {
        widthScale = bounds.x / textWidth;
    }

    if (bounds.y > 0.0f && textHeight > bounds.y)
    {
        heightScale = bounds.y / textHeight;
    }

    // Use the smaller scale factor to maintain aspect ratio
    if (widthScale < 1.0f || heightScale < 1.0f)
    {
        const float tempScale = std::min(widthScale, heightScale);

        scale *= tempScale;
        rowHeight *= tempScale;

        // Recalculate dimensions with new scale
        textWidth *= tempScale;
        textHeight *= tempScale;
    }

    // Adjust position for alignment if needed
    glm::vec2 alignmentOffset(0.0f);

    if (alignment != TextAlignment::TopLeft)
    {
        // Horizontal alignment
        switch (alignment)
        {
        case TextAlignment::TopCenter:
        case TextAlignment::MiddleCenter:
        case TextAlignment::BottomCenter:
            alignmentOffset.x = -textWidth * 0.5f;
            break;

        case TextAlignment::TopRight:
        case TextAlignment::MiddleRight:
        case TextAlignment::BottomRight:
            alignmentOffset.x = -textWidth;
            break;

        default:
            break;
        }

        // Vertical alignment
        switch (alignment)
        {
        case TextAlignment::MiddleLeft:
        case TextAlignment::MiddleCenter:
        case TextAlignment::MiddleRight:
            alignmentOffset.y = textHeight * 0.5f;
            break;

        case TextAlignment::BottomLeft:
        case TextAlignment::BottomCenter:
        case TextAlignment::BottomRight:
            alignmentOffset.y = textHeight;
            break;

        default:
            break;
        }

        x += alignmentOffset.x;
        y += alignmentOffset.y;
    }
    y -= rowHeight;

    // Second pass: render glyphs
    const float startX = x;

    for (uint32_t codepoint : codepoints)
    {
        if (codepoint == '\n')
        {
            y -= rowHeight;  // y increases downward
            x = startX;
            continue;
        }

        auto it = glyphs.find(codepoint);
        if (it == glyphs.end())
        {
            continue; // Should not happen since we filtered already
        }

        const Glyph& glyph = it->second;

        if (glyph.textureID)
        {
            float xpos = x + glyph.bearing.x * scale;
            float ypos = y - (glyph.size.y - glyph.bearing.y) * scale;  // y increases downward
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

void TextRenderer::updateProjectionMatrixInternal()
{
    TextRenderer& inst = getInstance();
    auto& proj = inst.projectionMatrix;
    const auto& textShader = inst.textShader;

    proj = glm::ortho(inst.left, inst.right, inst.bottom, inst.top);
    textShader.setMat4("projection", proj);
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

void TextRenderer::setGlyphInstanceBatchSize(size_t size)
{
    TextRenderer& inst = getInstance();
    inst.glyphInstanceBatchSize = size;
    if (size == 0)
    {
        inst.glyphInstances.reset();
        inst.textInstanceVBO.destroy();
    }
    else
    {
        inst.glyphInstances = std::unique_ptr<GlyphInstance[]>(new GlyphInstance[size]);
        inst.createInstanceVBO(size);
    }
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

void TextRenderer::renderText(const std::string& text, float x, float y, float rowHeight,
    const glm::vec3& color, TextAlignment alignment, const glm::vec2& bounds)
{
    renderTextInternal(reinterpret_cast<const void*>(text.c_str()), text.length(), decodeStdString, x, y, rowHeight, color, alignment, bounds);
}

void TextRenderer::renderText(const std::u8string& text, float x, float y, float rowHeight,
    const glm::vec3& color, TextAlignment alignment, const glm::vec2& bounds)
{
    renderTextInternal(reinterpret_cast<const void*>(text.c_str()), text.length(), decodeUTF8, x, y, rowHeight, color, alignment, bounds);
}

void TextRenderer::renderText(const std::u16string& text, float x, float y, float rowHeight,
    const glm::vec3& color, TextAlignment alignment, const glm::vec2& bounds)
{
    renderTextInternal(reinterpret_cast<const void*>(text.c_str()), text.length(), decodeUTF16, x, y, rowHeight, color, alignment, bounds);
}

void TextRenderer::renderText(const std::u32string& text, float x, float y, float rowHeight,
    const glm::vec3& color, TextAlignment alignment, const glm::vec2& bounds)
{
    renderTextInternal(reinterpret_cast<const void*>(text.c_str()), text.length(), decodeUTF32, x, y, rowHeight, color, alignment, bounds);
}

void TextRenderer::setPixelCoordinateSpace(int width, int height)
{
    setCustomCoordinateSpace(
        0.0f, static_cast<float>(width),
        0.0f, static_cast<float>(height)
    );
}

void TextRenderer::setCustomCoordinateSpace(float left, float right, float bottom, float top)
{
    TextRenderer& inst = getInstance();

    if (left >= right || bottom >= top)
    {
        std::cerr << "[TextRenderer][setCustomCoordinateSpace]: Invalid bounds\n";
        return;
    }

    inst.left = left;
    inst.right = right;
    inst.bottom = bottom;
    inst.top = top;

    inst.updateProjectionMatrixInternal();
}
