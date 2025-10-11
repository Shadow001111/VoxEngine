#pragma once
#include "Chunk/MeshData.h"
#include "Chunk/BlockData.h"
#include "Chunk/Metrics.h"

#include "Core/Int3.h"
#include "Core/Multithreading/ProcessingFence.h"

#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>

class Chunk
{
public:
	enum class State : uint8_t
	{
		NeedsBlocks = 0,
		BuildingBlocks,
		NeedsMesh,
		BuildingMesh,
		Ready
	};
private:
	static std::mutex meshUploadMutex;
	static std::vector<MeshData*> pendingMeshUploads;
private:
	Int3 position; // Chunk coordinates in chunk space

	std::atomic<State> state;
	std::atomic<bool> isLoadedInWorld{ false };
	std::atomic<bool> isLoadedChunkColumnData { false };
	std::atomic<bool> areBlocksBuilt{ false };

	Block blocks[CHUNK_VOLUME];

	MeshData meshData;
	ProcessingFence processingFence;

	static size_t getIndex(int x, int y, int z);
public:
	static BlockTextureIDDatabase blockTextureDatabase;

	Chunk* neighbors[6]; // Pointers to neighboring chunks, for easier access when building mesh

	Chunk();
	~Chunk();

	Chunk(const Chunk&) = delete;
	Chunk& operator=(const Chunk&) = delete;
	Chunk(Chunk&&) = delete;
	Chunk& operator=(Chunk&&) = delete;

	bool operator==(const Chunk& other) const;

	void init(int x, int y, int z, Chunk** neighbors);
	void destroy();

	void buildBlocks();
	void buildMesh();

	void render() const;
	bool canBeRendered() const;

	Block getBlock_inBoundaries(int x, int y, int z) const;
	Block getBlock_checkSideNeighbor(int x, int y, int z, int side) const;
	Block getBlock_checkNeighbors(int x, int y, int z) const;
	Block getBlock_checkNeighborsTraverse(int x, int y, int z) const;
private:
	int calculateVertexAO(bool side1, bool side2, bool corner) const;
	int calculateFaceAO(int x, int y, int z, int normal) const;
public:
	int getX() const;
	int getY() const;
	int getZ() const;
	Int3 getPosition() const;
	size_t getFaceCount() const;
	size_t getFaceCapacity() const;

	// Atomic getters and setters
	State getState() const;
	void setState(State newState);

	bool getIsProcessing() const;

	bool getIsLoadedInWorld() const;
public:

	// Static method to process all pending mesh uploads on main thread
	static void sendMeshesToGPU();
};