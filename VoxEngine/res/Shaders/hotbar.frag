#version 460 core

in vec2 texCoords;
            
uniform sampler2D uTexture;
            
out vec4 FragColor;
            
void main()
{
    FragColor = vec4(vec3(0.5), 1.0);//texture(uTexture, texCoords);
}