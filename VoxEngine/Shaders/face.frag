#version 460 core

in vec2 uv;
flat in float ao[4];

out vec4 FragColor;

float interpolateAO()
{
    float v0 = mix(ao[0], ao[1], uv.x);
    float v1 = mix(ao[3], ao[2], uv.x);
    float v = mix(v0, v1, uv.y);
    return v;
}

void main()
{
    float ao_ = interpolateAO();
    
    vec3 baseColor = vec3(1.0);
    vec3 shadedColor = baseColor * ao_;
    FragColor = vec4(shadedColor, 1.0);
}