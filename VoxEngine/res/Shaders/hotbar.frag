#version 460 core

in vec2 texCoords;
            
uniform sampler2DArray uTexture;
uniform uint uTextureId;
            
layout(location = 0) out vec4 FragColor;
            
void main()
{
    FragColor = texture(uTexture, vec3(texCoords, uTextureId));
}