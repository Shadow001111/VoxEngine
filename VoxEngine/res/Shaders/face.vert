#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in ivec2 instanceData;

layout(binding = 0) restrict readonly buffer chunkPositionSSBO
{
	int chunkPositions[];
};

uniform mat4 view;
uniform mat4 projection;
uniform ivec3 cameraChunkPosition;
uniform int CHUNK_SIZE;
uniform int skyLightSub = -15;

out vec2 uv;
out vec2 texCoords;
flat out vec4 ao;
flat out vec4 light;
flat out uint textureID;
out vec3 viewVertexPosition;

const mat3 vertexRotations[6] = mat3[6](
    mat3(0, 1, 0,
         0, 0, -1,
         0, 0, 0),
    mat3(0, 1, 0,
         0, 0, 1,
         0, 0, 0),
    mat3(1, 0, 0,
         0, 0, 1,
         0, 0, 0),
    mat3(-1, 0, 0,
         0, 0, 1,
         0, 0, 0),
    mat3(-1, 0, 0,
         0, 1, 0,
         0, 0, 0),
    mat3(1, 0, 0,
         0, 1, 0,
         0, 0, 0)
);

const vec3 vertexOffsets[6] = vec3[6](
    vec3(0, 0, 1),
    vec3(1, 0, 0),
    vec3(0, 0, 0),
    vec3(1, 1, 0),
    vec3(1, 0, 0),
    vec3(0, 0, 1)
);

const mat2 uvRotations[6] = mat2[6](
    mat2(0, 1,
         -1, 0),
    mat2(0, 1,
         -1, 0),
    mat2(-1, 0,
         0, 1),
    mat2(1, 0,
         0, 1),
    mat2(1, 0,
         0, 1),
    mat2(1, 0,
         0, 1)
);

const vec2 uvOffsets[6] = vec2[6](
    vec2(1, 0),
    vec2(1, 0),
    vec2(1, 0),
    vec2(0, 0),
    vec2(0, 0),
    vec2(0, 0)
);

const mat2 texCoordsRotations[4] = mat2[4](
    mat2(1, 0,
         0, 1),
    mat2(0, 1,
         -1, 0),
    mat2(-1, 0,
         0, -1),
    mat2(0, -1,
         1, 0)
);

const vec2 texCoordsOffsets[4] = vec2[4](
    vec2(0, 0),
    vec2(1, 0),
    vec2(1, 1),
    vec2(0, 1)
);

const float INV_LIGHT_SCALE = 1.0 / 15.0;
const float INV_AO_SCALE = 1.0 / 3.0;

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
    
    uint textureTransformation = (instanceData.x >> 30) & 3;

    int blockLight0 = (instanceData.y >> 0)  & 15;
    int skyLight0   = (instanceData.y >> 4)  & 15;
    int blockLight1 = (instanceData.y >> 8)  & 15;
    int skyLight1   = (instanceData.y >> 12) & 15;
    int blockLight2 = (instanceData.y >> 16) & 15;
    int skyLight2   = (instanceData.y >> 20) & 15;
    int blockLight3 = (instanceData.y >> 24) & 15;
    int skyLight3   = (instanceData.y >> 28) & 15;
    
    // AO
    ao.x = ao0 * INV_AO_SCALE;
    ao.y = ao1 * INV_AO_SCALE;
    ao.z = ao2 * INV_AO_SCALE;
    ao.w = ao3 * INV_AO_SCALE;

    // Light
    light.x = max(blockLight0, max(0, skyLight0 - skyLightSub)) * INV_LIGHT_SCALE;
    light.y = max(blockLight1, max(0, skyLight1 - skyLightSub)) * INV_LIGHT_SCALE;
    light.z = max(blockLight2, max(0, skyLight2 - skyLightSub)) * INV_LIGHT_SCALE;
    light.w = max(blockLight3, max(0, skyLight3 - skyLightSub)) * INV_LIGHT_SCALE;

    // Transform vertex and uv
    vec3 vertexPos = vertexRotations[normal] * vec3(aPos, 0.0) + vertexOffsets[normal];
    uv = uvRotations[normal] * aPos + uvOffsets[normal];

    // Chunk position
    const uint posIndex = uint(gl_DrawID) * 3u;
    const ivec3 chunkPosition = ivec3
	(
		chunkPositions[posIndex],
		chunkPositions[posIndex + 1u],
		chunkPositions[posIndex + 2u]
	);
    const ivec3 relativeChunkPosition = chunkPosition - cameraChunkPosition;

    // Transform texCoords. TODO: Remove branching. Idk, maybe it won't change a thing.
    texCoords = uv;
    if (textureTransformation > 0)
    {
        ivec3 blockWorldPos = ivec3(x, y, z) + ivec3(chunkPosition);
        uint hash = hash3(blockWorldPos);

        uint rotation = uint(textureTransformation > 1) * (hash & 3u);
        uint flip = (hash >> 3) & 3;

        vec2 flipMask = vec2(float((flip & 1u) == 0), float((flip & 2u) == 0));

        texCoords = mix(texCoords, vec2(1.0) - texCoords, flipMask);
        texCoords = texCoordsRotations[rotation] * texCoords + texCoordsOffsets[rotation];
    }

    //
    vec3 worldPos = vec3(CHUNK_SIZE * relativeChunkPosition) + vertexPos + vec3(x, y, z);
    vec4 viewPos = view * vec4(worldPos, 1.0);

    viewVertexPosition = viewPos.xyz;
    
    gl_Position = projection * viewPos;
}