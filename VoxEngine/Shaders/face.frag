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

//float interpolateAO()
//{
//    // Minecraft-style triangular interpolation
//    // Choose diagonal based on which corners have stronger AO
//    // This prevents the "box" artifact
//    
//    float ao0 = ao[0];
//    float ao1 = ao[1];
//    float ao2 = ao[2];
//    float ao3 = ao[3];
//    
//    // Check if we should flip the diagonal
//    // Compare opposite corners: if ao0 + ao2 is darker than ao1 + ao3,
//    // use the 0-2 diagonal, otherwise use 1-3 diagonal
//    bool flipDiagonal = (ao0 + ao2) > (ao1 + ao3);
//    
//    float result;
//    
//    if (flipDiagonal)
//    {
//        // Use diagonal from bottom-left (0) to top-right (2)
//        if (uv.x > uv.y)
//        {
//            // Lower-right triangle (vertices 1, 2, 0)
//            result = ao1 + (ao2 - ao1) * uv.y + (ao0 - ao1) * (1.0 - uv.x);
//        }
//        else
//        {
//            // Upper-left triangle (vertices 0, 3, 2)
//            result = ao0 + (ao3 - ao0) * uv.y + (ao2 - ao0) * uv.x;
//        }
//    }
//    else
//    {
//        // Use diagonal from top-left (3) to bottom-right (1)
//        if (uv.x < 1.0 - uv.y)
//        {
//            // Lower-left triangle (vertices 0, 1, 3)
//            result = ao0 + (ao1 - ao0) * uv.x + (ao3 - ao0) * uv.y;
//        }
//        else
//        {
//            // Upper-right triangle (vertices 1, 2, 3)
//            result = ao2 + (ao1 - ao2) * (1.0 - uv.y) + (ao3 - ao2) * (1.0 - uv.x);
//        }
//    }
//    
//    return result;
//}

void main()
{
    vec3 baseColor = vec3(1.0);
    vec3 shadedColor = baseColor * interpolateAO();

    float inverseFogEffect = clamp(exp(-pow(depth * fogDensity, fogGradient)), 0.0, 1.0);
	vec3 fogProcessedColor = mix(fogColor, shadedColor, inverseFogEffect);

    FragColor = vec4(fogProcessedColor, 1.0);
}