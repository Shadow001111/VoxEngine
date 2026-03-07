#pragma once
#include "ChunkInstancedMeshFaceStorage.h"

class Chunk;

class ChunkMesh
{
	friend class Chunk;

	ChunkInstancedMeshFaceStorage faceStorage;

	static DynamicArray<ChunkInstancedMeshFaceStorage*> pendingMeshUploads;
public:
	static void sendMeshesToGPU();
};

