#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in uvec2 instanceData;

layout(binding = 0) restrict readonly buffer chunkPositionSSBO
{
	int chunkPositions[];
};

uniform mat4 view;
uniform mat4 projection;
uniform float CHUNK_SIZE;

out vec2 uv;
out vec2 texCoords;
flat out float ao[4];
flat out float light[4];
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

const float aoValues[4] = float[4](
    0.0,
    0.089,
    0.409,
    1.0
);

const float INV_LIGHT_SCALE = 1.0 / 15.0;

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
    uint x = instanceData.x & 15u;
    uint y = (instanceData.x >> 4u) & 15u;
    uint z = (instanceData.x >> 8u) & 15u;

    uint normal = (instanceData.x >> 12u) & 7u;

    uint faceAO = (instanceData.x >> 15u) & 255u;
    uint ao0 = faceAO & 3u;
    uint ao1 = (faceAO >> 2u) & 3u;
    uint ao2 = (faceAO >> 4u) & 3u;
    uint ao3 = faceAO >> 6u;

    textureID = (instanceData.x >> 23u) & 127u;
    
    uint textureTransformation = (instanceData.x >> 30u) & 3u;

    uint blockLight0 = (instanceData.y >> 0u)  & 15u;
    uint skyLight0   = (instanceData.y >> 4u)  & 15u;
    uint blockLight1 = (instanceData.y >> 8u)  & 15u;
    uint skyLight1   = (instanceData.y >> 12u) & 15u;
    uint blockLight2 = (instanceData.y >> 16u) & 15u;
    uint skyLight2   = (instanceData.y >> 20u) & 15u;
    uint blockLight3 = (instanceData.y >> 24u) & 15u;
    uint skyLight3   = (instanceData.y >> 28u) & 15u;
    
    // AO
    ao[0] = aoValues[ao0];
    ao[1] = aoValues[ao1];
    ao[2] = aoValues[ao2];
    ao[3] = aoValues[ao3];

    // Light
    light[0] = max(blockLight0, skyLight0) * INV_LIGHT_SCALE;
    light[1] = max(blockLight1, skyLight1) * INV_LIGHT_SCALE;
    light[2] = max(blockLight2, skyLight2) * INV_LIGHT_SCALE;
    light[3] = max(blockLight3, skyLight3) * INV_LIGHT_SCALE;

    // Transform vertex and uv
    vec3 vertexPos = vertexRotations[normal] * vec3(aPos, 0.0) + vertexOffsets[normal];
    uv = uvRotations[normal] * aPos + uvOffsets[normal];

    // Chunk position
    const uint posIndex = uint(gl_DrawID) * 3u;
    const vec3 chunkPosition = CHUNK_SIZE * vec3
	(
		chunkPositions[posIndex],
		chunkPositions[posIndex + 1u],
		chunkPositions[posIndex + 2u]
	);

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
    vec3 worldPos = chunkPosition + vertexPos + vec3(x, y, z);
    vec4 viewPos = view * vec4(worldPos, 1.0);

    viewVertexPosition = viewPos.xyz;
    
    gl_Position = projection * viewPos;
}