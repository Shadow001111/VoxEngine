#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in ivec2 instanceData;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 chunkPosition;

out vec2 uv;
out vec2 texCoords;
flat out float ao[4];
flat out float light[4];
flat out uint textureID;

out float depth;

uint hash3(ivec3 sv)
{
    uvec3 v = sv;
    uint x = v.x * 0x27d4eb2du + v.y * 0x165667b1u + v.z * 0x1b873593u;
    x ^= x >> 15u;
    x *= 0x85ebca6bu;
    x ^= x >> 13u;
    x *= 0xc2b2ae35u;
    x ^= x >> 16u;
    return x;
}

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

    textureID = (instanceData.x >> 23) & 127;
    
    int textureTransformation = (instanceData.x >> 30) & 3;

    int blockLight0 = (instanceData.y >> 0) & 15;
    int skyLight0 = (instanceData.y >> 4) & 15;
    int blockLight1 = (instanceData.y >> 8) & 15;
    int skyLight1 = (instanceData.y >> 12) & 15;
    int blockLight2 = (instanceData.y >> 16) & 15;
    int skyLight2 = (instanceData.y >> 20) & 15;
    int blockLight3 = (instanceData.y >> 24) & 15;
    int skyLight3 = (instanceData.y >> 28) & 15;
    
    // AO
    ao[0] = float(ao0) / 3.0;
    ao[1] = float(ao1) / 3.0;
    ao[2] = float(ao2) / 3.0;
    ao[3] = float(ao3) / 3.0;

    // Light
    light[0] = max(blockLight0, skyLight0) / 15.0;
    light[1] = max(blockLight1, skyLight1) / 15.0;
    light[2] = max(blockLight2, skyLight2) / 15.0;
    light[3] = max(blockLight3, skyLight3) / 15.0;

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

    // Transform texCoords
    texCoords = uv;
    if (textureTransformation > 0)
    {
        uint hash = hash3(ivec3(x + chunkPosition.x, y + chunkPosition.y, z + chunkPosition.z));

        // Flip
        uint flip = (hash >> 3u) & 3u;
        if ((flip & 1u) == 0u)
        {
            texCoords.x = 1.0 - texCoords.x;
        }
        if ((flip & 2u) == 0u)
        {
            texCoords.y = 1.0 - texCoords.y;
        }

        if (textureTransformation > 1)
        {
            uint rotation = hash & 3u;
            if (rotation == 0u)
            {
                texCoords = texCoords;
            }
            else if (rotation == 1u)
            {
                texCoords = vec2(1.0 - texCoords.y, texCoords.x);
            }
            else if (rotation == 2u)
            {
                texCoords = vec2(1.0 - texCoords.x, 1.0 - texCoords.y);
            }
            else
            {
                texCoords = vec2(texCoords.y, 1.0 - texCoords.x);
            }
        }
    }

    //
    vec3 worldPos = chunkPosition + vertexPos + vec3(x, y, z);
    vec4 viewPos = view * vec4(worldPos, 1.0);
    
    depth = length(viewPos.xyz);
    
    gl_Position = projection * viewPos;
}