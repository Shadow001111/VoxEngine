#pragma once
#include "ChunkMeshFaceStorage.h"

class Chunk;

class ChunkMesh
{
	friend class Chunk;

	ChunkMeshFaceStorage faceStorage;

	static DynamicArray<Chunk*> pendingMeshUploads;
public:
	static void sendMeshesToGPU();
};

