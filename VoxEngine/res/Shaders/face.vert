#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in ivec2 instanceData;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 chunkPosition;

out vec2 uv;
flat out float ao[4];
flat out float light[4];
flat out uint textureID;

out float depth;

void main()
{
    // Unpack face data
    int x = instanceData.x & 15;
    int y = (instanceData.x >> 4) & 15;
    int z = (instanceData.x >> 8) & 15;

    int normal = (instanceData.x >> 12) & 7;

    int faceAO = (instanceData.x >> 15) & 255;
    int ao0 = faceAO & 3;
    int ao1 = (faceAO >> 2) & 3;
    int ao2 = (faceAO >> 4) & 3;
    int ao3 = faceAO >> 6;

    textureID = (instanceData.x >> 23) & 511;

    int light0 = instanceData.y & 255;
    int light1 = (instanceData.y >> 8) & 255;
    int light2 = (instanceData.y >> 16) & 255;
    int light3 = instanceData.y >> 24;
    
    // AO
    ao[0] = float(ao0) / 3.0;
    ao[1] = float(ao1) / 3.0;
    ao[2] = float(ao2) / 3.0;
    ao[3] = float(ao3) / 3.0;

    // Light
    light[0] = max(light0 & 15, light0 >> 4) / 15.0;
    light[1] = max(light1 & 15, light1 >> 4) / 15.0;
    light[2] = max(light2 & 15, light2 >> 4) / 15.0;
    light[3] = max(light3 & 15, light3 >> 4) / 15.0;

    // Move quad to face
    vec3 vertexPos = vec3(0.0);

    // TODO: Maybe remove branching
    if (normal == 0) //  -x
    {
        vertexPos = vec3(0.0, aPos.x, 1.0 - aPos.y);
        uv = vec2(1.0 - aPos.y, aPos.x);
    }
    else if (normal == 1) // +x
    {
        vertexPos = vec3(1.0, aPos.x, aPos.y);
        uv = vec2(1.0 - aPos.y, aPos.x);
    }
    else if (normal == 2) // -y
    {
        vertexPos = vec3(aPos.x, 0.0, aPos.y);
        uv = vec2(1.0 - aPos.x, aPos.y);
    }
    else if (normal == 3) // +y
    {
        vertexPos = vec3(1.0f - aPos.x, 1.0f, aPos.y);
        uv = vec2(aPos.x, aPos.y);
    }
    else if (normal == 4) // -z
    {
        vertexPos = vec3(1.0 - aPos.x, aPos.y, 0.0);
        uv = vec2(aPos.x, aPos.y);
    }
    else // +z
    {
        vertexPos = vec3(aPos.x, aPos.y, 1.0);
        uv = vec2(aPos.x, aPos.y);
    }

    //
    vec3 worldPos = chunkPosition + vertexPos + vec3(x, y, z);
    vec4 viewPos = view * vec4(worldPos, 1.0);
    
    depth = length(viewPos.xyz);
    
    gl_Position = projection * viewPos;
}