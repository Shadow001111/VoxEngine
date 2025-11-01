#version 460 core

uniform sampler2DArray blockTextures; // sRGB

uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogGradient;

in vec2 uv;
in vec2 texCoords;
flat in float ao[4];
flat in float light[4];
flat in uint textureID;
in vec3 viewVertexPosition;


out vec4 FragColor;

float interpolateAO_Triang()
{
    float ao0 = ao[0];
    float ao1 = ao[1];
    float ao2 = ao[2];
    float ao3 = ao[3];

    float diag0 = mix(
        (1.0 - uv.x) * ao0 + (uv.x - uv.y) * ao1 + uv.y * ao2,
        (1.0 - uv.y) * ao0 + uv.x * ao2 + (uv.y - uv.x) * ao3,
        step(uv.x, uv.y)
    );

    float diag1 = mix(
        (1.0 - uv.x - uv.y) * ao0 + uv.x * ao1 + uv.y * ao3,
        (1.0 - uv.y) * ao1 + (uv.x + uv.y - 1.0) * ao2 + (1.0 - uv.x) * ao3,
        step(1.0, uv.x + uv.y)
    );

    float useDiagonal = step((ao1 + ao3) - (ao0 + ao2), 0.0);
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

    FragColor = vec4(colorWithFog, textureColor.a);
}
