#pragma once
#include "ChunkInstancedMeshFaceStorage.h"

class Chunk;

class ChunkMesh
{
	friend class Chunk;

	ChunkInstancedMeshFaceStorage faceStorage;

	static std::vector<ChunkInstancedMeshFaceStorage*> pendingMeshUploads;
	static std::atomic<bool> hasPendingMeshUploads;
public:
	static void sendMeshesToGPU();

	static bool getHasPendingMeshUploads() { return hasPendingMeshUploads.load(std::memory_order_acquire); }
	static void setHasPendingMeshUploads(bool value) { return hasPendingMeshUploads.store(value, std::memory_order_release); }
};

