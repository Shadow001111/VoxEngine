#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in int instanceData;

void main()
{
    gl_Position = vec4(aPos + vec2(instanceData * 1.1, 0), 0.0, 1.0);
}