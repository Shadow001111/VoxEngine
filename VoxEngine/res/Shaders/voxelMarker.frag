#version 460 core

uniform vec3 color;

in vec2 uv;

out vec4 FragColor;

void main()
{
    vec2 dpos = uv - 0.5;
    float distSq = dot(dpos, dpos);
    if (distSq < 0.25)
    {
        discard;
    }
    FragColor = vec4(color, 0.25);
}