#version 460 core

uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogGradient;

uniform sampler2DArray blockTextures;

in vec2 uv;
flat in float ao[4];
flat in uint textureID;
flat in float light;

in float depth;

out vec4 FragColor;

float interpolateAO_Quad()
{
    float v0 = mix(ao[0], ao[1], uv.x);
    float v1 = mix(ao[3], ao[2], uv.x);
    return mix(v0, v1, uv.y);
}

float interpolateAO_Triang()
{
    // Get the four AO values at the corners
    float ao0 = ao[0];
    float ao1 = ao[1];
    float ao2 = ao[2];
    float ao3 = ao[3];

    // Compute AO for diagonal 0-2
    float diag0;
    diag0 = mix(
        (1.0 - uv.x) * ao0 + (uv.x - uv.y) * ao1 + uv.y * ao2, // uv.x > uv.y
        (1.0 - uv.y) * ao0 + uv.x * ao2 + (uv.y - uv.x) * ao3, // uv.x <= uv.y
        step(uv.x, uv.y)
    );

    // Compute AO for diagonal 1-3
    float diag1;
    diag1 = mix(
        (1.0 - uv.x - uv.y) * ao0 + uv.x * ao1 + uv.y * ao3,          // uv.x + uv.y < 1
        (1.0 - uv.y) * ao1 + (uv.x + uv.y - 1.0) * ao2 + (1.0 - uv.x) * ao3, // uv.x + uv.y >= 1
        step(1.0, uv.x + uv.y)
    );

    // Determine which diagonal to use
    float useDiagonal = step((ao1 + ao3) - (ao0 + ao2), 0.0);

    // Branchless selection of the diagonal
    float result = mix(diag1, diag0, useDiagonal);

    return result;
}

void main()
{
    vec3 baseColor = texture(blockTextures, vec3(uv, textureID)).xyz;
    vec3 shadedColor = baseColor * interpolateAO_Triang() * light;

    float inverseFogEffect = clamp(exp(-pow(depth * fogDensity, fogGradient)), 0.0, 1.0);
	vec3 fogProcessedColor = mix(fogColor, shadedColor, inverseFogEffect);

    FragColor = vec4(fogProcessedColor, 1.0);
}