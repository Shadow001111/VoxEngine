#version 460 core

uniform vec3 color;

in vec2 uv;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 dpos = uv - 0.5;
    float distSq = dot(dpos, dpos);
    if (distSq < 0.25)
    {
        discard;
    }
    fragColor = vec4(color, 1.0);
}