#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in int instanceData;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 chunkPosition;

out vec2 uv;
flat out float ao[4];

void main()
{
    // Unpack face data
    int x = instanceData & 15;
    int y = (instanceData >> 4) & 15;
    int z = (instanceData >> 8) & 15;

    int normal = (instanceData >> 12) & 7;

    int faceAO = (instanceData >> 15) & 255;

    // Unpack all AO values
    int ao0 = faceAO & 3;
    int ao1 = (faceAO >> 2) & 3;
    int ao2 = (faceAO >> 4) & 3;
    int ao3 = faceAO >> 6;
    
    // Fix anisotropy by flipping diagonal if needed
    // Check if we need to flip the quad orientation
    int vertexIndex = gl_VertexID % 4;
    int aoValue;

    ao[0] = float(ao0) / 3.0;
    ao[1] = float(ao1) / 3.0;
    ao[2] = float(ao2) / 3.0;
    ao[3] = float(ao3) / 3.0;

    // Move quad to face
    vec3 vertexPos = vec3(0.0);
    vec2 vertexUV = vec2(0.0);

    if (normal == 0) //  -x
    {
        vertexPos = vec3(0.0, aPos.x, 1.0 - aPos.y);
        vertexUV = vec2(1.0 - aPos.y, aPos.x);
    }
    else if (normal == 1) // +x
    {
        vertexPos = vec3(1.0, aPos.x, aPos.y);
        vertexUV = vec2(1.0 - aPos.y, aPos.x);
    }
    else if (normal == 2) // -y
    {
        vertexPos = vec3(aPos.x, 0.0, aPos.y);
        vertexUV = vec2(1.0 - aPos.x, aPos.y);
    }
    else if (normal == 3) // +y
    {
        vertexPos = vec3(1.0f - aPos.x, 1.0f, aPos.y);
        vertexUV = vec2(aPos.x, aPos.y);
    }
    else if (normal == 4) // -z
    {
        vertexPos = vec3(1.0 - aPos.x, aPos.y, 0.0);
        vertexUV = vec2(aPos.x, aPos.y);
    }
    else // +z
    {
        vertexPos = vec3(aPos.x, aPos.y, 1.0);
        vertexUV = vec2(aPos.x, aPos.y);
    }

    //
    uv = vertexUV;

    vec3 worldPos = chunkPosition + vertexPos + vec3(x, y, z);
    gl_Position = projection * view * vec4(worldPos, 1.0);
}