#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

uniform mat4 view;
uniform mat4 projection;
uniform ivec3 cameraChunkPosition;
uniform int CHUNK_SIZE;

uniform vec3 position;
uniform float scale;

out vec2 uv;

void main()
{
    uv = aUV;

    vec3 vertexPos = aPos * scale;
    vec3 worldPos = position - vec3(cameraChunkPosition * CHUNK_SIZE) + vertexPos;
    vec4 viewPos = view * vec4(worldPos, 1.0);
    
    gl_Position = projection * viewPos;
}