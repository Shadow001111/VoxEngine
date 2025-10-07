#pragma once
#include "BlockData.h"
#include "Metrics.h"

#include "Int3.h"

#include <glad/glad.h>

#include <vector>
#include <mutex>
#include <atomic>

struct BlockFaceInstance
{
	int32_t data;

	BlockFaceInstance(int x, int y, int z, int normal, int ao, int textureID);
};

class Chunk
{
public:
	enum class State
	{
		NeedsBlocks = 0,
		BuildingBlocks,
		NeedsMesh,
		BuildingMesh,
		Ready
	};
private:
	struct PendingMeshUpload
	{
		std::vector<BlockFaceInstance> instances;
		GLuint instanceVBO;
		Chunk* chunk; // for face capacity. TODO: Should be removed

		PendingMeshUpload(std::vector<BlockFaceInstance>&& instances, GLuint instanceVBO, Chunk* chunk);
	};

	static std::mutex meshUploadMutex;
	static std::vector<PendingMeshUpload> pendingMeshUploads;
private:
	Int3 position; // Chunk coordinates in chunk space
	Block blocks[CHUNK_VOLUME];

	GLuint vao, vbo, instanceVBO; // Buffers
	uint32_t faceCount;
	uint32_t faceCapacity;

	bool loadedChunkColumnData;

	std::atomic<State> state;

	std::atomic<bool> beingProcessed;

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

	// Atomic getters and setters
	State getState() const;
	void setState(State newState);

	bool isBeingProcessed() const;
private:
	void setIsBeingProcessed(bool value);
public:
	// Static method to process all pending mesh uploads on main thread
	static void sendMeshesToGPU();

	// Debug
	uint32_t getFaceCount() const;
	uint32_t getFaceCapacity() const;
};