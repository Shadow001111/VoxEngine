#version 460 core

in vec2 texCoords;

layout(location = 0) out vec4 fragColor;

uniform sampler2D sampleTexture;
uniform vec2 nearFar;

float linearizeDepth(float depth, float near, float far)
{
    return (2.0 * near) / (far + near - depth * (far - near));
}

void main()
{
    float depth_ = texture(sampleTexture, texCoords).r;
    depth_ = linearizeDepth(depth_, nearFar.x, nearFar.y);
    fragColor = vec4(depth_, depth_, depth_, 1.0);
}