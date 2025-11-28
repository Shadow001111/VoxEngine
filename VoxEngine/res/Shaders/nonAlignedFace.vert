#version 460 core

layout(location = 0) in vec2 aPos;

layout(location = 1) in uint blockPositionAndUs;
layout(location = 2) in uvec2 vertexShifts;
layout(location = 3) in uint instanceVsAndTextureId;
layout(location = 4) in uvec2 instanceLight;
layout(location = 5) in uint instanceAO;

layout(binding = 0) restrict readonly buffer chunkPositionSSBO
{
	int chunkPositions[];
};

uniform mat4 view;
uniform mat4 projection;
uniform ivec3 cameraChunkPosition;
uniform int skyLightSub = 0; // TODO: Maybe make it float for smooth sky light transition

out vec2 uv;
flat out uint textureID;
out vec3 viewVertexPosition;
flat out float[8] blockVertexLightData;
out vec3 vertexLocalPos;

const float INV_LIGHT_SCALE = 1.0 / 15.0;
const float INV_AO_SCALE = 1.0 / 3.0;
const float AO_RANGE = 0.9; // [0; 1]

uint getBlockLight(uint data, int i)
{
    return bitfieldExtract(data, i * 8, 4);
}

uint getSkyLight(uint data, int i)
{
    return bitfieldExtract(data, i * 8 + 4, 4);
}

uint getAO(uint data, int i)
{
    return bitfieldExtract(data, i * 2, 2);
}

void main()
{
    // Unpack face data
    uvec3 blockPosition, vertexSubBlockPosition;
    uint u, v;
    int[8] blockLight;
    int[8] skyLight;
    float[8] faceAO;

    blockPosition.x = bitfieldExtract(blockPositionAndUs, 0, 4);
    blockPosition.y = bitfieldExtract(blockPositionAndUs, 4, 4);
    blockPosition.z = bitfieldExtract(blockPositionAndUs, 8, 4);
    u = bitfieldExtract(blockPositionAndUs, 12 + gl_VertexID * 5, 5);

    uint vertexShiftPage = vertexShifts[gl_VertexID >> 1];
    int vertexShiftIndex = (gl_VertexID & 1) * 15; // MUST be int, not uint
    vertexSubBlockPosition.x = bitfieldExtract(vertexShiftPage, vertexShiftIndex     , 5);
    vertexSubBlockPosition.y = bitfieldExtract(vertexShiftPage, vertexShiftIndex +  5, 5);
    vertexSubBlockPosition.z = bitfieldExtract(vertexShiftPage, vertexShiftIndex + 10, 5);

    v = bitfieldExtract(instanceVsAndTextureId, gl_VertexID * 5, 5);
    textureID = bitfieldExtract(instanceVsAndTextureId, 20, 12);

    blockLight[0] = int(getBlockLight(instanceLight.x, 0));
    blockLight[1] = int(getBlockLight(instanceLight.x, 1));
    blockLight[2] = int(getBlockLight(instanceLight.x, 2));
    blockLight[3] = int(getBlockLight(instanceLight.x, 3));
    blockLight[4] = int(getBlockLight(instanceLight.y, 0));
    blockLight[5] = int(getBlockLight(instanceLight.y, 1));
    blockLight[6] = int(getBlockLight(instanceLight.y, 2));
    blockLight[7] = int(getBlockLight(instanceLight.y, 3));

    skyLight[0] = int(getSkyLight(instanceLight.x, 0)) - skyLightSub;
    skyLight[1] = int(getSkyLight(instanceLight.x, 1)) - skyLightSub;
    skyLight[2] = int(getSkyLight(instanceLight.x, 2)) - skyLightSub;
    skyLight[3] = int(getSkyLight(instanceLight.x, 3)) - skyLightSub;
    skyLight[4] = int(getSkyLight(instanceLight.y, 0)) - skyLightSub;
    skyLight[5] = int(getSkyLight(instanceLight.y, 1)) - skyLightSub;
    skyLight[6] = int(getSkyLight(instanceLight.y, 2)) - skyLightSub;
    skyLight[7] = int(getSkyLight(instanceLight.y, 3)) - skyLightSub;

    faceAO[0] = 1.0 - AO_RANGE + float(getAO(instanceAO, 0)) * INV_AO_SCALE * AO_RANGE;
    faceAO[1] = 1.0 - AO_RANGE + float(getAO(instanceAO, 1)) * INV_AO_SCALE * AO_RANGE;
    faceAO[2] = 1.0 - AO_RANGE + float(getAO(instanceAO, 2)) * INV_AO_SCALE * AO_RANGE;
    faceAO[3] = 1.0 - AO_RANGE + float(getAO(instanceAO, 3)) * INV_AO_SCALE * AO_RANGE;
    faceAO[4] = 1.0 - AO_RANGE + float(getAO(instanceAO, 4)) * INV_AO_SCALE * AO_RANGE;
    faceAO[5] = 1.0 - AO_RANGE + float(getAO(instanceAO, 5)) * INV_AO_SCALE * AO_RANGE;
    faceAO[6] = 1.0 - AO_RANGE + float(getAO(instanceAO, 6)) * INV_AO_SCALE * AO_RANGE;
    faceAO[7] = 1.0 - AO_RANGE + float(getAO(instanceAO, 7)) * INV_AO_SCALE * AO_RANGE;

    for (int i = 0; i < 8; i++)
    {
        blockVertexLightData[i] = max(blockLight[i], skyLight[i]) * INV_LIGHT_SCALE * faceAO[i];
    }

    vertexLocalPos = vec3(vertexSubBlockPosition) / 16.0;
    vec3 vertexPos = vertexLocalPos + vec3(blockPosition);
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
    vec3 worldPos = vec3(16.0 * relativeChunkPosition) + vertexPos;
    vec4 viewPos = view * vec4(worldPos, 1.0);

    viewVertexPosition = viewPos.xyz;
    
    gl_Position = projection * viewPos;
}