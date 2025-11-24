#version 460 core

uniform sampler2DArray blockTextures;

uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogGradient;

in vec2 uv;
//flat in vec4 ao;
//flat in vec4 light;
flat in uint textureID;
in vec3 viewVertexPosition;

out vec4 FragColor;

void main()
{
    vec4 textureColor = texture(blockTextures, vec3(uv, textureID));

    vec3 baseColor = textureColor.rgb;

    vec3 shadedColor = baseColor;// * interpolateAO_Triang() * interpolateLight_Quad();

    float depth = length(viewVertexPosition);
    float fogFactor = exp(-pow((depth * fogDensity), fogGradient));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    vec3 colorWithFog = mix(fogColor, shadedColor, fogFactor);

    FragColor = vec4(colorWithFog, textureColor.a);
}
