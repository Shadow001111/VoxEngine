#version 460 core

uniform sampler2DArray blockTextures;

uniform float farPlane;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogGradient;

in vec2 uv;
//flat in vec4 ao;
//flat in vec4 light;
flat in uint textureID;
in vec3 viewVertexPosition;

layout (location = 0) out vec4 accumulation;
layout (location = 1) out float revealage;

float weight(float z, float alpha)
{
    return alpha * max(1e-2, 3e3 * pow(1.0 - z * 0.9, 10.0));
}

void main()
{
    vec4 textureColor = texture(blockTextures, vec3(uv, textureID));

    vec3 baseColor = textureColor.rgb;

    vec3 shadedColor;// = baseColor * interpolateAO_Triang() * interpolateLight_Quad();

    float depth = length(viewVertexPosition);
    float fogFactor = exp(-pow((depth * fogDensity), fogGradient));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    vec3 colorWithFog = mix(fogColor, shadedColor, fogFactor);

    float w = weight(depth / farPlane, textureColor.a);

    accumulation = vec4(colorWithFog, 1.0) * (textureColor.a * w);
    revealage = textureColor.a;
}