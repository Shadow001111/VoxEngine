#version 460 core

layout(location = 0) in vec2 aPos;

layout(location = 1) in vec3 instPos0;
layout(location = 2) in vec3 instPos1;
layout(location = 3) in vec3 instPos2;
layout(location = 4) in vec3 instPos3;

layout(location = 5) in vec2 instUV0;
layout(location = 6) in vec2 instUV1;
layout(location = 7) in vec2 instUV2;
layout(location = 8) in vec2 instUV3;

layout(location = 9) in uint instTextureID;

layout(binding = 0) restrict readonly buffer chunkPositionSSBO
{
	int chunkPositions[];
};

uniform mat4 view;
uniform mat4 projection;
uniform ivec3 cameraChunkPosition;
uniform int CHUNK_SIZE;
uniform int skyLightSub = 0; // TODO: Maybe make it float for smooth sky light transition

out vec2 uv;
flat out uint textureID;
out vec3 viewVertexPosition;

uint hash3(ivec3 sv)
{
    uvec3 v = uvec3(sv);
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
    vec3 vertexPos;
    if (gl_VertexID == 0)
    {
        vertexPos = instPos0;
        uv = instUV0;
    }
    else if (gl_VertexID == 1)
    {
        vertexPos = instPos1;
        uv = instUV1;
    }
    else if (gl_VertexID == 2)
    {
        vertexPos = instPos2;
        uv = instUV2;
    }
    else
    {
        vertexPos = instPos3;
        uv = instUV3;
    }

    textureID = instTextureID;

    // Chunk position
    const uint posIndex = uint(gl_DrawID) * 3u;
    const ivec3 chunkPosition = ivec3
	(
		chunkPositions[posIndex],
		chunkPositions[posIndex + 1u],
		chunkPositions[posIndex + 2u]
	);
    const ivec3 relativeChunkPosition = chunkPosition - cameraChunkPosition;

    //
    vec3 worldPos = vec3(CHUNK_SIZE * relativeChunkPosition) + vertexPos;
    vec4 viewPos = view * vec4(worldPos, 1.0);

    viewVertexPosition = viewPos.xyz;
    
    gl_Position = projection * viewPos;
}