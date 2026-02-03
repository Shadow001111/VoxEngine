#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in uvec2 instanceData;

layout(std430, binding = 0) restrict readonly buffer chunkPositionSSBO
{
	int chunkPositions[];
};

layout(std430, binding = 1) restrict readonly buffer chunkNormalSSBO
{
	int chunkNormals[];
};

uniform mat4 view;
uniform mat4 projection;
uniform ivec3 cameraChunkPosition;
uniform float skyLightSub;

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

const mat2 texCoordsRotations[2] = mat2[2](
    mat2(1, 0,
         0, 1),
    mat2(0, 1,
         -1, 0)
);

const vec2 texCoordsOffsets[2] = vec2[2](
    vec2(0, 0),
    vec2(1, 0)
);

const float INV_LIGHT_SCALE = 1.0 / 15.0;
const float INV_AO_SCALE = 1.0 / 3.0;
const float AO_RANGE = 0.9; // [0; 1]

uint getNormal(uint index)
{
    // 10 normals of 3 bits are stored in each uint
    
    uint uintIndex = index / 10u;
    uint normalIndexInUint = index - uintIndex * 10u;
    
    // Calculate the bit position (3 bits per normal)
    uint bitPos = normalIndexInUint * 3u;
    
    // Extract 3 bits starting from bitPos
    uint packedNormal = chunkNormals[uintIndex];
    uint normal = (packedNormal >> bitPos) & 0x7u;
    
    return normal;
}

void main()
{
    // Extract spatial data
    uint x = bitfieldExtract(instanceData.x, 0, 4);
    uint y = bitfieldExtract(instanceData.x, 4, 4);
    uint z = bitfieldExtract(instanceData.x, 8, 4);

    // Extract face orientation
    uint normal = getNormal(gl_DrawID);

    // Extract AO
    vec4 faceAO = vec4(
        bitfieldExtract(instanceData.x, 12, 2),
        bitfieldExtract(instanceData.x, 14, 2),
        bitfieldExtract(instanceData.x, 16, 2),
        bitfieldExtract(instanceData.x, 18, 2)
    );

    // Extract texture data
    textureID = bitfieldExtract(instanceData.x, 20, 9);
    uint textureTransformation = bitfieldExtract(instanceData.x, 29, 3);

    // Extract lighting
    vec4 blockLight = vec4(
        bitfieldExtract(instanceData.y, 0, 4),
        bitfieldExtract(instanceData.y, 8, 4),
        bitfieldExtract(instanceData.y, 16, 4),
        bitfieldExtract(instanceData.y, 24, 4)
    );

    vec4 skyLight = vec4(
        bitfieldExtract(instanceData.y, 4, 4),
        bitfieldExtract(instanceData.y, 12, 4),
        bitfieldExtract(instanceData.y, 20, 4),
        bitfieldExtract(instanceData.y, 28, 4)
    ) - skyLightSub; // NOTE: Can be negative, I don't care because blockLight is always greater than 0
    
    // AO: Convert from [0,3] range to [0.1, 1.0]
    ao = vec4(1.0 - AO_RANGE) + faceAO * INV_AO_SCALE * AO_RANGE;

    // Lighting: Max of block vs sky light, normalized to [0,1]
    light = max(blockLight, skyLight) * INV_LIGHT_SCALE;

    // Transform vertex and uv based on face normal
    vec3 vertexPos = vertexRotations[normal] * vec3(aPos, 0.0) + vertexOffsets[normal];
    uv = uvRotations[normal] * aPos + uvOffsets[normal];

    // Get chunk position from SSBO
    const uint posIndex = uint(gl_DrawID) * 3u;
    const ivec3 chunkPosition = ivec3
	(
		chunkPositions[posIndex],
		chunkPositions[posIndex + 1u],
		chunkPositions[posIndex + 2u]
	);
    const ivec3 relativeChunkPosition = chunkPosition - cameraChunkPosition;

    // Apply texture transformations (flip/rotation)
    vec2 flip = vec2(textureTransformation & 1u, (textureTransformation >> 1u) & 1u);
    uint rotation = (textureTransformation >> 2u) & 1u;
    texCoords = uv;
    texCoords = mix(texCoords, vec2(1.0) - texCoords, flip);
    texCoords = texCoordsRotations[rotation] * texCoords + texCoordsOffsets[rotation];

    // Calculate final world position
    vec3 worldPos = vec3(16.0 * relativeChunkPosition) + vertexPos + vec3(x, y, z);
    
    vec4 viewPos = view * vec4(worldPos, 1.0);
    viewVertexPosition = viewPos.xyz;
    gl_Position = projection * viewPos;
}