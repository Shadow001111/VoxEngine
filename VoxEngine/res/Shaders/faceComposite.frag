#version 460 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D accumulationTex;
uniform sampler2D revealageTex;
uniform sampler2D opaqueTex; // Regular opaque scene

void main()
{
    vec4 accumulation = texture(accumulationTex, texCoords);
    float revealage = texture(revealageTex, texCoords).r;
    vec4 opaque = texture(opaqueTex, texCoords);
    
    // Composite transparent over opaque
    vec3 average_color = accumulation.rgb / max(accumulation.a, 1e-5);
    vec4 transparent = vec4(average_color, 1.0 - revealage);
    
    // Blend transparent over opaque
    FragColor = mix(opaque, transparent, transparent.a);
}