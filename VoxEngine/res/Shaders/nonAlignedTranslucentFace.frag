#version 460 core

uniform sampler2DArray blockTextures;

uniform float farPlane;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogGradient;

in vec2 uv;
flat in uint textureID;
in vec3 viewVertexPosition;
flat in float[8] blockVertexLightData;
in vec3 vertexLocalPos;

layout (location = 0) out vec4 accumulation;
layout (location = 1) out float revealage;

float weight(float z)
{
    return max(1e-2, 3e3 * pow(1.0 - z * 0.9, 10.0));
}

float interpolateLight()
{
    float x = vertexLocalPos.x;
    float y = vertexLocalPos.y;
    float z = vertexLocalPos.z;

    float ix = 1.0 - x;
    float iy = 1.0 - y;
    float iz = 1.0 - z;

    float w0 = ix * iy * iz; // (0,0,0)
    float w1 = x  * iy * iz; // (1,0,0)
    float w2 = x  * iy * z;  // (1,0,1)
    float w3 = ix * iy * z;  // (0,0,1)
    float w4 = ix * y  * iz; // (0,1,0)
    float w5 = x  * y  * iz; // (1,1,0)
    float w6 = x  * y  * z;  // (1,1,1)
    float w7 = ix * y  * z;  // (0,1,1)

    return w0 * blockVertexLightData[0] +
           w1 * blockVertexLightData[1] +
           w2 * blockVertexLightData[2] +
           w3 * blockVertexLightData[3] +
           w4 * blockVertexLightData[4] +
           w5 * blockVertexLightData[5] +
           w6 * blockVertexLightData[6] +
           w7 * blockVertexLightData[7];
}

void main()
{
    vec4 textureColor = texture(blockTextures, vec3(uv, textureID));

    vec3 baseColor = textureColor.rgb;

    vec3 shadedColor = baseColor * interpolateLight();

    float depth = length(viewVertexPosition);
    float fogFactor = exp(-pow((depth * fogDensity), fogGradient));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    vec3 colorWithFog = mix(fogColor, shadedColor, fogFactor);

    float w = weight(depth / farPlane);

    accumulation = vec4(colorWithFog, 1.0) * (textureColor.a * w);
    revealage = textureColor.a;
}