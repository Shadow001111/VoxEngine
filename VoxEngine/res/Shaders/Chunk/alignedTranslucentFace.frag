#version 460 core
#extension GL_ARB_bindless_texture : require

uniform sampler2DArray blockTextures;

uniform float farPlane;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogGradient;

in vec2 uv;
in vec2 texCoords;
flat in vec4 ao;
flat in vec4 light;
flat in uint textureID;
in vec3 viewVertexPosition;

layout(location = 2) out vec4 accumulation;
layout(location = 3) out float revealage;

float interpolateAO_Triang()
{
    float diag0 = mix(
        (1.0 - uv.x) * ao.x + (uv.x - uv.y) * ao.y + uv.y * ao.z,
        (1.0 - uv.y) * ao.x + uv.x * ao.z + (uv.y - uv.x) * ao.w,
        step(uv.x, uv.y)
    );

    float diag1 = mix(
        (1.0 - uv.x - uv.y) * ao.x + uv.x * ao.y + uv.y * ao.w,
        (1.0 - uv.y) * ao.y + (uv.x + uv.y - 1.0) * ao.z + (1.0 - uv.x) * ao.w,
        step(1.0, uv.x + uv.y)
    );

    float useDiagonal = step((ao.y + ao.w) - (ao.x + ao.z), 0.0);
    return mix(diag1, diag0, useDiagonal);
}

float interpolateLight_Quad()
{
    float v0 = mix(light[0], light[1], uv.x);
    float v1 = mix(light[3], light[2], uv.x);
    return mix(v0, v1, uv.y);
}

float weight(float z)
{
    return max(1e-2, 3e3 * pow(1.0 - z * 0.9, 10.0));
}

void main()
{
    vec4 textureColor = texture(blockTextures, vec3(texCoords, textureID));

    vec3 baseColor = textureColor.rgb;

    float shade = interpolateAO_Triang() * interpolateLight_Quad();
    vec3 shadedColor = baseColor * shade;

    float depth = length(viewVertexPosition);
    float fogFactor = exp(-pow((depth * fogDensity), fogGradient));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    vec3 colorWithFog = mix(fogColor, shadedColor, fogFactor);

    float w = weight(depth / farPlane);

    accumulation = vec4(colorWithFog, 1.0) * (textureColor.a * w);
    revealage = textureColor.a;
}