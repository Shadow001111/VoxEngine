#pragma once
#include "ChunkMeshFaceStorage.h"

class Chunk;

class ChunkMesh
{
	friend class Chunk;

	ChunkMeshFaceStorage faceStorage;

	static DynamicArray<Chunk*> pendingMeshUploads;
public:
	using Flag = ChunkMeshFaceStorage::Flag;

	static void sendMeshesToGPU();

	void setFlag(Flag flag, bool value) noexcept { faceStorage.setFlag(flag, value); }
	bool readFlag(Flag flag) const noexcept { return faceStorage.readFlag(flag); }
	bool readAndSetFlag(Flag flag, bool value) noexcept { return faceStorage.readAndSetFlag(flag, value); }
};

