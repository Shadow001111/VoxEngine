#version 460 core

in vec2 texCoords;

layout(location = 0) out vec4 fragColor;

layout(location = 0) uniform sampler2D colorTexture;

void main()
{
    vec3 baseColor = texture(colorTexture, texCoords).xyz;

    fragColor = vec4(baseColor, 1.0);
}