#include "TextRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Game/TracyProfiler.h"
#include "Core/Decoding/UTFDecoder.h"

#include "Game/ProfileCategories.h"

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

void TextRenderer::loadGlyphs(FT_Face& face, Font& font)
{
    TRACY_SCOPE_NC("Load glyphs", ProfileCategory::General);
    
    static_assert(sizeof(FT_ULong) == sizeof(uint32_t), "FT_Ulong != uint32_t");

    font.glyphs.reserve(font.glyphCount);

    FT_UInt gindex;
    FT_ULong charcode = FT_Get_First_Char(face, &gindex);

    uint32_t textureIdCounter = 1;

    const bool isTextureCompressed = font.textureArray.isCompressed();

    const size_t layerSize = font.maxGlyphSize.x * font.maxGlyphSize.y;
    std::vector<uint8_t> textureData(layerSize * font.glyphCount, 0);

    while (gindex != 0)
    {
        {
            TRACY_SCOPE_NC("Load char", ProfileCategory::General);
            if (FT_Load_Char(face, charcode, FT_LOAD_RENDER))
            {
                std::cerr << "[TextRenderer]: Failed to load glyph: '" << charcode << "'.\n";
                continue;
            }
        }

        uint32_t glyphTextureId = 0;
        if (charcode > ' ')
        {
            TRACY_SCOPE_NC("Process char", ProfileCategory::General);

            glyphTextureId = textureIdCounter++;

            const int srcW = (int)face->glyph->bitmap.width;
            const int srcH = (int)face->glyph->bitmap.rows;

            uint8_t* destSlice = textureData.data() + (glyphTextureId - 1) * layerSize;
            const uint8_t* srcBuffer = face->glyph->bitmap.buffer;

            for (int row = 0; row < srcH; row++)
            {
                std::memcpy(
                    destSlice + (row * font.maxGlyphSize.x),
                    srcBuffer + (row * srcW),
                    srcW);
            }
        }

        Glyph glyph =
        {
            glyphTextureId,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            (GLuint)face->glyph->advance.x
        };

        font.glyphs.emplace(charcode, glyph);

        {
            TRACY_SCOPE_NC("Next char", ProfileCategory::General);
            charcode = FT_Get_Next_Char(face, charcode, &gindex);
        }
    }

    {
        TRACY_SCOPE_NC("Upload data to gpu", ProfileCategory::General);
        font.textureArray.uploadSubData3D(
            textureData.data(),
            0, 0, 0,
            font.maxGlyphSize.x, font.maxGlyphSize.y, textureIdCounter - 1,
            GL_UNSIGNED_BYTE
        );
    }
}

void TextRenderer::renderTextInternal(const void* text, size_t textLength, UTFDecoderFunction decoder, float x, float y, float rowHeight,
    const glm::vec3& color, TextAlignment alignment, const glm::vec2& bounds)
{
    TRACY_SCOPE_NC("Render text", ProfileCategory::Render);

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
    static std::vector<uint32_t> codepoints;
    codepoints.clear();
    codepoints.reserve(textLength);

    size_t index = 0;
    float maxLineWidth = 0.0f;
    int lineCount = 1;
    float currentLineWidth = 0.0f;

    const uint32_t replaceCodepoint = '?';
    const bool doesNotReplaceCodepointExist = glyphs.find(replaceCodepoint) == glyphs.end();

    {
        TRACY_SCOPE_NC("Decode text", ProfileCategory::General);

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

    // Render glyphs
    const float startX = x;

    {
        TRACY_SCOPE_NC("Render glyphs", ProfileCategory::Render);
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
    TRACY_SCOPE_NC("Init text renderer", ProfileCategory::General);

    getInstance();
}

bool TextRenderer::loadFont(const std::string& fontName, GLuint fontSize)
{
    TRACY_SCOPE_NC("Load font", ProfileCategory::General);

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
    {
        TRACY_SCOPE_NC("Init FreeType", ProfileCategory::General);
        if (FT_Init_FreeType(&ft))
        {
            std::cerr << "[TextRenderer][loadFont]: Couldn't init FreeType Library\n";
            return false;
        }
    }

    const std::string fontPath = "res/fonts/" + fontName + ".ttf";

    FT_Face face;
    {
        TRACY_SCOPE_NC("Loading font", ProfileCategory::General);
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
        {
            std::cerr << "[TextRenderer][loadFont]: Failed to load font: " << fontPath << "\n";
            FT_Done_FreeType(ft);
            return false;
        }
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Create font
    Font font;
    font.fontSize = fontSize;

    // Get font info
    font.glyphCount = face->num_glyphs;
    {
        float scale = (float)fontSize / (float)face->units_per_EM;
        font.maxGlyphSize.x = (int)std::ceil((face->bbox.xMax - face->bbox.xMin) * scale);
        font.maxGlyphSize.y = (int)std::ceil((face->bbox.yMax - face->bbox.yMin) * scale);
    }

    // Create texture array
    GLint oldAlignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldAlignment);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	GLenum internalFormat = TextureCompression::resolveInternalFormat(TextureCompression::Format::AUTO, TextureCompression::Channels::R, false);

    // Block-compressed formats operate on 4x4 pixel blocks.
    // The texture dimensions must be multiples of 4 so every glyph
    // upload can be padded to a valid block-aligned size.
    if (Texture::isFormatCompressed(internalFormat))
    {
        font.maxGlyphSize.x = (font.maxGlyphSize.x + 3) & ~3;
        font.maxGlyphSize.y = (font.maxGlyphSize.y + 3) & ~3;
    }

    {
        TRACY_SCOPE_NC("Create 2d array", ProfileCategory::General);
        font.textureArray.create2DArray(font.maxGlyphSize.x, font.maxGlyphSize.y, font.glyphCount, internalFormat);
    }

    {
        Texture::Parameters textureParametrs
        {
            .minFilter = GL_NEAREST,
            .magFilter = GL_NEAREST,
            .wrapS = GL_CLAMP_TO_EDGE,
            .wrapT = GL_CLAMP_TO_EDGE
        };

        font.textureArray.setParameters(textureParametrs);
    }

    // Load glyphs
    loadGlyphs(face, font);

	// Make bindless if supported
    if (Texture::getExtensions().bindless)
    {
        font.textureArray.initHandle();
        font.textureArray.makeResident();
	}

	// Report
    std::cout
        << "[TextRenderer][loadFont]: Loaded font: '" << fontName << "' (" << fontPath << "). Character count: " << font.glyphs.size()
        << ". Max glyph size: (" << font.maxGlyphSize.x << ", " << font.maxGlyphSize.y << ").\n";

	// Free FreeType resources
    {
        TRACY_SCOPE_NC("Free FreeType resources", ProfileCategory::General);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    }

	// Restore alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, oldAlignment);

	// Store font
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

    Font* previousFont = inst.currentFont;
    inst.currentFont = &it->second;


    // Pass texture handle to shader
    if (Texture::getExtensions().bindless)
    {
        inst.textShader.setHandleui64ARB("glyphTextureArray", inst.currentFont->textureArray.getHandle());
    }
    
	// Make previous font texture non-resident if needed
    if (previousFont && previousFont != inst.currentFont && Texture::getExtensions().bindless)
    {
        previousFont->textureArray.makeNonResident();
    }
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
	if (!Texture::getExtensions().bindless)
    {
        font->textureArray.bindUnit(0);
    }

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
