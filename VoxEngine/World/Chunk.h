#pragma once
#include "BlockData.h"
#include "Metrics.h"

#include "Int3.h"
#include "Multithreading/ProcessingFence.h"

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
	enum class State : uint8_t
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
		uint16_t faceCapacity;
		bool ready;

		std::vector<BlockFaceInstance> instances;

		ProcessingFence processingFence;

		MeshData();
		~MeshData();

		void resetFaceCount();
		void allocateMemoryForBuffer(size_t faceCount);
	public:
		size_t getFaceCountSum() const;
		size_t getFaceCapacity() const;
		bool isReady() const;
	};

	static std::mutex meshUploadMutex;
	static std::vector<MeshData*> pendingMeshUploads;
private:
	Int3 position; // Chunk coordinates in chunk space

	std::atomic<State> state;
	std::atomic<bool> isLoadedInWorld{ false };
	bool loadedChunkColumnData = false;

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

	// Atomic getters and setters
	State getState() const;
	void setState(State newState);

	bool getIsProcessing() const;

	bool getIsLoadedInWorld() const;
	void setIsLoadedInWorld(bool value);

	// Static method to process all pending mesh uploads on main thread
	static void sendMeshesToGPU();

	// Debug
	const MeshData& getMeshData() const;
};