#version 460 core

in vec2 texCoords;
flat in uint textureID;

out vec4 FragColor;

uniform sampler2DArray glyphTextureArray;
uniform vec3 textColor;

void main()
{
    float alpha = texture(glyphTextureArray, vec3(texCoords, textureID)).r;
    FragColor = vec4(textColor, alpha);
}