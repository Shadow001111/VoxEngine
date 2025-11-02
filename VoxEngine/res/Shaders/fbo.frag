#version 460 core

in vec2 texCoords;

out vec4 FragColor;

uniform sampler2D colorTexture;
uniform sampler2D depthTexture;

uniform float gamma = 2.2;

float linearizeDepth(float depth, vec2 nearFar)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearFar.x * nearFar.y) / (nearFar.y + nearFar.x - z * (nearFar.y - nearFar.x));
}

void main()
{
    vec3 baseColor = texture(colorTexture, texCoords).xyz;

    //vec3 gammaCorrected = pow(baseColor, vec3(1.0 / gamma));

    FragColor = vec4(baseColor, 1.0);
}