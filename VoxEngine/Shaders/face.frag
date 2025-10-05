#version 460 core

uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogGradient;

in vec2 uv;
flat in float ao[4];
in float depth;

out vec4 FragColor;

float interpolateAO()
{
    float v0 = mix(ao[0], ao[1], uv.x);
    float v1 = mix(ao[3], ao[2], uv.x);
    return mix(v0, v1, uv.y);
}

void main()
{
    vec3 baseColor = vec3(1.0);
    vec3 shadedColor = baseColor * interpolateAO();

    float inverseFogEffect = clamp(exp(-pow(depth * fogDensity, fogGradient)), 0.0, 1.0);
	vec3 fogProcessedColor = mix(fogColor, shadedColor, inverseFogEffect);

    FragColor = vec4(fogProcessedColor, 1.0);
}