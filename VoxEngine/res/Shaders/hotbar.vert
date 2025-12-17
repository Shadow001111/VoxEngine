#version 460 core

layout (location = 0) in vec4 posAndUV;

out vec2 texCoords;

uniform mat4 projection;
uniform mat4 model;

void main()
{
    texCoords = posAndUV.zw;
    vec4 position = vec4(posAndUV.xy, 0.0, 1.0);
    gl_Position = projection * model * position;
}