#version 460 core

layout (location = 0) in vec4 posAndUV;

out vec2 texCoords;

uniform mat4 model;
uniform mat4 projection;

void main()
{
    texCoords = posAndUV.zw;
    gl_Position = projection * model * vec4(posAndUV.xy, 0.0, 1.0);
}