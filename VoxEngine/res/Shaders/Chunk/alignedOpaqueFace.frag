#version 460 core
#extension GL_ARB_bindless_texture : require

uniform sampler2DArray blockTextures;

uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogGradient;

in vec2 uv;
in vec2 texCoords;
flat in vec4 ao;
flat in vec4 light;
flat in uint textureID;
in vec3 viewVertexPosition;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out float geometryAlpha;

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

void main()
{
    vec4 textureColor = texture(blockTextures, vec3(texCoords, textureID));

    vec3 baseColor = textureColor.rgb;

    vec3 shadedColor = baseColor * interpolateAO_Triang() * interpolateLight_Quad();

    float depth = length(viewVertexPosition);
    float fogFactor = exp(-pow((depth * fogDensity), fogGradient));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    vec3 colorWithFog = mix(fogColor, shadedColor, fogFactor);

    fragColor = vec4(colorWithFog, 1.0);
    geometryAlpha = 1.0;
}
