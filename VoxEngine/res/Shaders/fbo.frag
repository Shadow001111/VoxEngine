#version 460 core

in vec2 texCoords;

out vec4 FragColor;

uniform sampler2D colorTexture;
uniform sampler2D depthTexture;
uniform vec2 nearFarPlanes;

void main()
{
    vec3 baseColor = texture(colorTexture, texCoords).xyz;

    FragColor = vec4(baseColor, 1.0);
}