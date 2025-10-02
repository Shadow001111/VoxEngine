#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in int instanceData;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    float dist = sqrt(instanceData);
    float angle = instanceData * 0.5;

    float x = cos(angle) * dist;
    float y = sin(angle) * dist;

    vec3 worldPos = vec3(aPos + vec2(x, y), 0.0);
    gl_Position = projection * view * vec4(worldPos, 1.0);
}