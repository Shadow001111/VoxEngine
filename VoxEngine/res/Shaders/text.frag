#version 460 core

in vec2 uv;

out vec4 FragColor;

uniform sampler2D characterTexture;
uniform vec3 textColor;

void main()
{
    float alpha = texture(characterTexture, uv).r;
    FragColor = vec4(textColor, alpha);
}