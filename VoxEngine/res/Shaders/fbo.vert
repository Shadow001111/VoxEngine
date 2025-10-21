#version 460 core

layout (location = 0) in vec4 posAndUV;

out vec2 texCoords;

void main()
{
    texCoords = posAndUV.zw;
    gl_Position = vec4(posAndUV.xy, 0.0, 1.0);
}