#pragma once
#include "ChunkInstancedMeshFaceStorage.h"

class Chunk;

class ChunkMesh
{
	friend class Chunk;

	ChunkInstancedMeshFaceStorage faceStorage;

	static DynamicArray<Chunk*> pendingMeshUploads;
public:
	static void sendMeshesToGPU();
};

