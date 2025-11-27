#version 460 core

layout(location = 0) in vec2 aPos;

layout(location = 1) in uvec4 instancePositionsAndUs;
layout(location = 2) in uint instanceVsAndTextureId;

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

void decodeXYZU(uint data, out uint x, out uint y, out uint z, out uint u)
{
    x = bitfieldExtract(data,  0, 9);
    y = bitfieldExtract(data,  9, 9);
    z = bitfieldExtract(data, 18, 9);
    u = bitfieldExtract(data, 27, 5);
}

void decodeVandTexture(uint data, out uint v, out uint texID)
{
    v    = bitfieldExtract(data,  gl_VertexID * 5, 5);
    texID = bitfieldExtract(data, 20, 12);
}

void main()
{
    uint x, y, z, u, v;
    decodeXYZU(instancePositionsAndUs[gl_VertexID], x, y, z, u);
    decodeVandTexture(instanceVsAndTextureId, v, textureID);

    vec3 vertexPos = vec3(x, y, z) / 16.0;
    uv = vec2(u, v) / 16.0;

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