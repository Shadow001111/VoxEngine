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
	class MeshData
	{
		friend Chunk;

		GLuint vao, vbo, instanceVBO;
		uint16_t faceCount[6]; // Count for each side
		// TODO: Try to have only one int as 'faceCount', not an array
		uint16_t faceCapacity;

		std::vector<BlockFaceInstance> instances[6]; // TODO: Maybe should have one vector? Just combine 6 vectors. 6 times less gpu buffer write calls.

		bool ready; // TODO: Remove this thing. Just move faceCount setter to sendMeshesToGPU

		MeshData();
		~MeshData();

		void resetFaceCount();
		void updateCapacityIfNeeded(size_t faceCount);
	public:
		uint16_t getFaceCountSum() const;
		uint16_t getFaceCapacity() const;
		bool isReady() const;
	};

	static std::mutex meshUploadMutex;
	static std::vector<MeshData*> pendingMeshUploads;
private:
	Int3 position; // Chunk coordinates in chunk space
	Block blocks[CHUNK_VOLUME];

	MeshData meshData;

	std::atomic<State> state;
	std::atomic<bool> beingProcessed;

	bool loadedChunkColumnData;

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
	const MeshData& getMeshData() const;
};