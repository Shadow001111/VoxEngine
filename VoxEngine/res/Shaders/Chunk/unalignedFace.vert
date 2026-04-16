#version 460 core

layout(location = 0) in vec2 aPos;

layout(location = 1) in uvec4 faceData0; // words 0..3
layout(location = 2) in uvec4 faceData1; // words 4..7
layout(location = 3) in uvec4 faceData2; // words 8..11

layout(binding = 0) restrict readonly buffer chunkPositionSSBO
{
	int chunkPositions[];
};

uniform mat4 view;
uniform mat4 projection;
uniform ivec3 cameraChunkPosition;
uniform float skyLightSub = 0;

out vec2 uv;
flat out uint textureID;
out vec3 viewVertexPosition;
out vec3 vertexLocalPos;
flat out vec4 blockVertexLightData[6];
flat out vec4 blockVertexAOData[6];

const float INV_LIGHT_SCALE = 1.0 / 15.0;
const float INV_AO_SCALE = 1.0 / 3.0;
const float AO_RANGE = 0.9; // [0; 1]

uint getWord(int index)
{
    switch (index)
    {
        case 0:  return faceData0.x;
        case 1:  return faceData0.y;
        case 2:  return faceData0.z;
        case 3:  return faceData0.w;

        case 4:  return faceData1.x;
        case 5:  return faceData1.y;
        case 6:  return faceData1.z;
        case 7:  return faceData1.w;

        case 8:  return faceData2.x;
        case 9:  return faceData2.y;
        case 10: return faceData2.z;
        case 11: return faceData2.w;
    }

    return 0u; // safety fallback
}

int getBits(uint word, int offset, int count)
{
    return int(bitfieldExtract(word, offset, count));
}

uint getAO(uint word, int i)
{
    return bitfieldExtract(word, i * 2, 2);
}

void main()
{
    // Fetch packed words
    uint w0  = getWord(0);
    uint w1  = getWord(1);
    uint w2  = getWord(2);
    uint w3  = getWord(3);

    // Unpack block position and UVs
    uvec3 blockPosition;
    blockPosition.x = bitfieldExtract(w0, 0, 4);
    blockPosition.y = bitfieldExtract(w0, 4, 4);
    blockPosition.z = bitfieldExtract(w0, 8, 4);

    uint u = bitfieldExtract(w0, 12 + int(gl_VertexID) * 5, 5);
    uint v = bitfieldExtract(w3, int(gl_VertexID) * 5, 5);
    textureID = bitfieldExtract(w3, 20, 12);

    // Unpack per-vertex local position
    uint shiftWord = (gl_VertexID < 2u) ? w1 : w2;
    int shiftBase  = int(gl_VertexID & 1u) * 15;

    uvec3 vertexSubBlockPosition;
    vertexSubBlockPosition.x = bitfieldExtract(shiftWord, shiftBase + 0, 5);
    vertexSubBlockPosition.y = bitfieldExtract(shiftWord, shiftBase + 5, 5);
    vertexSubBlockPosition.z = bitfieldExtract(shiftWord, shiftBase + 10, 5);

    // Lights: 24 values, 4 per word, 8 bits each (low nibble = block, high nibble = sky)
    int blockLight[8];
    int skyLight[8];
    float faceAO[8];

    uint lightWords[6] = uint[6](
        getWord(4), getWord(5), getWord(6),
        getWord(7), getWord(8), getWord(9)
    );

    uint aoWords[2] = uint[2](
        getWord(10), getWord(11)
    );

    for (int i = 0; i < 24; i++)
    {
        // Light
        uint lw = lightWords[i >> 2]; // divide by 4
        int lane = i & 3;

        int blockLight = int(bitfieldExtract(lw, lane * 8, 4));
        int skyLight   = int(bitfieldExtract(lw, lane * 8 + 4, 4)) - int(skyLightSub);

        // AO
        uint aw = aoWords[i >> 4];
        int aoLane = i & 15;

        float ao = 1.0 - AO_RANGE +
                   float(bitfieldExtract(aw, aoLane * 2, 2)) *
                   INV_AO_SCALE * AO_RANGE;

        //
        blockVertexLightData[i >> 2][lane] = max(blockLight, skyLight) * INV_LIGHT_SCALE;
        blockVertexAOData[i >> 2][lane] = ao;
    }

    //
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