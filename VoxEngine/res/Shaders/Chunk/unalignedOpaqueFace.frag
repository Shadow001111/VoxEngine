#version 460 core
#extension GL_ARB_bindless_texture : require

uniform sampler2DArray blockTextures;

uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogGradient;

in vec2 uv;
flat in uint textureID;
in vec3 viewVertexPosition;
in vec3 vertexLocalPos;

flat in vec4 blockVertexLightData[6];
flat in vec4 blockVertexAOData[6];

layout(location = 0) out vec4 fragColor;
layout(location = 1) out float geometryAlpha;

const float INV_LIGHT_SCALE = 1.0 / 15.0;
const float INV_AO_SCALE    = 1.0 / 3.0;
const float AO_RANGE        = 0.9;

float interpolateLight_Quad(vec4 light, vec2 faceUV)
{
    float v0 = mix(light.x, light.y, faceUV.x);
    float v1 = mix(light.w, light.z, faceUV.x);
    return mix(v0, v1, faceUV.y);
}

float interpolateAO_Triang(vec4 ao, vec2 faceUV)
{
    float diag0 = mix(
        (1.0 - faceUV.x) * ao.x + (faceUV.x - faceUV.y) * ao.y + faceUV.y * ao.z,
        (1.0 - faceUV.y) * ao.x + faceUV.x * ao.z + (faceUV.y - faceUV.x) * ao.w,
        step(faceUV.x, faceUV.y)
    );

    float diag1 = mix(
        (1.0 - faceUV.x - faceUV.y) * ao.x + faceUV.x * ao.y + faceUV.y * ao.w,
        (1.0 - faceUV.y) * ao.y + (faceUV.x + faceUV.y - 1.0) * ao.z + (1.0 - faceUV.x) * ao.w,
        step(1.0, faceUV.x + faceUV.y)
    );

    float useDiagonal = step((ao.y + ao.w) - (ao.x + ao.z), 0.0);
    return mix(diag1, diag0, useDiagonal);
}

vec2 faceUV_Xm(vec3 p) { return vec2(p.z, p.y); }
vec2 faceUV_Xp(vec3 p) { return vec2(1.0 - p.z, p.y); }
vec2 faceUV_Ym(vec3 p) { return vec2(p.x, 1.0 - p.z); } // TODO: Not sure about that
vec2 faceUV_Yp(vec3 p) { return vec2(1.0 - p.x, p.z); }
vec2 faceUV_Zm(vec3 p) { return vec2(1.0 - p.x, p.y); }
vec2 faceUV_Zp(vec3 p) { return vec2(p.x, p.y); }

void main()
{
    // Get vertex position and face normal
    vec3 p = clamp(vertexLocalPos, 0.0, 1.0);
    vec3 n = normalize(cross(dFdx(vertexLocalPos), dFdy(vertexLocalPos)));

    // Calculate weights
    vec3 w = normalize(abs(n));

    //
    float faceLight[6];
    float faceAO[6];

    for (int face = 0; face < 6; face++)
    {
        vec2 fuv;

        if (face == 0)      fuv = faceUV_Xm(p);
        else if (face == 1) fuv = faceUV_Xp(p);
        else if (face == 2) fuv = faceUV_Ym(p);
        else if (face == 3) fuv = faceUV_Yp(p);
        else if (face == 4) fuv = faceUV_Zm(p);
        else                fuv = faceUV_Zp(p);

        faceLight[face] = interpolateLight_Quad(
            blockVertexLightData[face],
            fuv
        );

        faceAO[face] = interpolateAO_Triang(
            blockVertexAOData[face],
            fuv
        );
    }

    //
    float lightX = mix(faceLight[0], faceLight[1], step(0.0, n.x));
    float lightY = mix(faceLight[2], faceLight[3], step(0.0, n.y));
    float lightZ = mix(faceLight[4], faceLight[5], step(0.0, n.z));
    
    float finalLight =
        lightX * w.x +
        lightY * w.y +
        lightZ * w.z;
    
    float aoX = mix(faceAO[0], faceAO[1], step(0.0, n.x));
    float aoY = mix(faceAO[2], faceAO[3], step(0.0, n.y));
    float aoZ = mix(faceAO[4], faceAO[5], step(0.0, n.z));
    
    float finalAO =
        aoX * w.x +
        aoY * w.y +
        aoZ * w.z;

    vec4 textureColor = texture(blockTextures, vec3(uv, textureID));
    vec3 shadedColor = textureColor.rgb * finalLight * finalAO;

    float depth = length(viewVertexPosition);
    float fogFactor = exp(-pow((depth * fogDensity), fogGradient));
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    vec3 colorWithFog = mix(fogColor, shadedColor, fogFactor);

    fragColor = vec4(colorWithFog, 1.0);
    geometryAlpha = 1.0;
}