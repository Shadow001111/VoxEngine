#include "TextRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

// TODO: Make text renderer instanced. Put textures either in texture array or texture atlas.
// TODO: Support UNICODE
// TODO: Don't render spaces. Don't generate texture for it.
// TODO: Add newlines. Add param from newline spacing.

Character::Character(GLuint textureID, const glm::ivec2& size, const glm::ivec2& bearing, GLuint advance) :
    textureID(textureID), size(size), bearing(bearing), advance(advance)
{
}

Character::~Character()
{
    if (textureID)
    {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }
}

Character::Character(Character&& other) noexcept :
    textureID(other.textureID), size(other.size), bearing(other.bearing), advance(other.advance)
{
    other.textureID = 0;
}

Character& Character::operator=(Character&& other) noexcept
{
    if (this != &other)
    {
        textureID = other.textureID;
        other.textureID = 0;
    }
    return *this;
}

Font::Font(Font&& other) noexcept :
    characters(std::move(other.characters)), fontSize(other.fontSize)
{
}

Font& Font::operator=(Font&& other) noexcept
{
    if (this != &other)
    {
        characters = std::move(other.characters);
        fontSize = other.fontSize;
    }
    return *this;
}


TextRenderer& TextRenderer::getInstance()
{
	static TextRenderer textRenderer;
	return textRenderer;
}

TextRenderer::TextRenderer()
{
    // Shaders
	std::vector<Shader::ShaderSource> textShaderSources =
	{
		{GL_VERTEX_SHADER, "res/Shaders/text.vert"},
		{GL_FRAGMENT_SHADER, "res/Shaders/text.frag"}
	};
	textShader = std::make_unique<Shader>(textShaderSources);
    textShader->use();
    textShader->setInt("characterTexture", 1);
    textShader->setMat4("projection", projectionMatrix);

    // Buffers
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);

    glBindVertexArray(textVAO);

    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
}

TextRenderer::~TextRenderer()
{
    fonts.clear();

    glDeleteBuffers(1, &textVBO);
    glDeleteVertexArrays(1, &textVAO);
}

void TextRenderer::init()
{
	getInstance();
}

bool TextRenderer::loadFont(const std::string& fontName, GLuint fontSize)
{
	TextRenderer& inst = getInstance();
	auto& fonts = inst.fonts;

	// Test if font exists
    // TODO: Check for font size, maybe overwrite old font
	if (fonts.find(fontName) != fonts.end())
	{
		std::cerr << "[TextRenderer]: Font '" << fontName << "' already exists." << std::endl;
		return false;
	}

    // Init FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cerr << "[TextRenderer]: Couldn't init FreeType Library." << std::endl;
        return false;
    }

    std::string fontPath = "res/fonts/" + fontName + ".ttf";

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
    {
        std::cerr << "[TextRenderer]: Failed to load font: " << fontPath << "." << std::endl;
        FT_Done_FreeType(ft);
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    Font font;
    font.characters.reserve(128);
    for (unsigned char c = 0; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cerr << "[TextRenderer]: Failed to load Glyph: '" << c << "'." << std::endl;
            continue;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            (GLuint)face->glyph->advance.x
        };

        font.characters.emplace(c, std::move(character));
    }
    font.fontSize = fontSize;

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    fonts.emplace(fontName, std::move(font));

    std::cout << "[TextRenderer]: Loaded font: '" << fontName << "' (" << fontPath << ")." << std::endl;
    return true;
}

void TextRenderer::setCurrentFont(const std::string& fontName)
{
    TextRenderer& inst = getInstance();
    auto& fonts = inst.fonts;

    auto it = fonts.find(fontName);
    if (it == fonts.end())
    {
        std::cerr << "[TextRenderer]: Font '" << fontName << "' isn't loaded." << std::endl;
        return;
    }

    inst.currentFont = &it->second;
}

void TextRenderer::renderText(const std::string& text, float x, float y, float rowHeight, const glm::vec3& color)
{
    TextRenderer& inst = getInstance();
    
    // Font
    const Font* font = inst.currentFont;
    if (!font)
    {
        std::cerr << "[TextRenderer]: Current font isn't set." << std::endl;
        return;
    }

    const auto& characters = font->characters;
    const float scale = rowHeight / font->fontSize;

    // Shader
    const auto& textShader = inst.textShader;
    textShader->use();
    textShader->setVec3("textColor", color.x, color.y, color.z);

    const auto& textVAO = inst.textVAO;
    const auto& textVBO = inst.textVBO;
    glBindVertexArray(textVAO);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE1);

    for (const char& c : text)
    {
        auto it = characters.find(c);
        if (it == characters.end()) continue;
        const Character& ch = it->second;

        GLfloat xpos = x + ch.bearing.x * scale;
        GLfloat ypos = y - (ch.size.y - ch.bearing.y) * scale;
        GLfloat w = ch.size.x * scale;
        GLfloat h = ch.size.y * scale;

        const float vertices[4][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        x += (ch.advance >> 6) * scale;
    }
}

void TextRenderer::updateProjectionMatrix(int width, int height)
{
    TextRenderer& inst = getInstance();
    auto& proj = inst.projectionMatrix;
    const auto& textShader = inst.textShader;

    proj = glm::ortho(0.0f, (float)width, 0.0f, (float)height);;
    textShader->use();
    textShader->setMat4("projection", proj);
}
