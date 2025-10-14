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
#include <queue>

struct LightNode
{
	int x, y, z;
	uint8_t lightLevel;
	int8_t propagationSide;

	LightNode(int x, int y, int z, uint8_t lightLevel, int8_t propagationSide);
};


class Chunk
{
public:
	enum class State : uint8_t
	{
		NeedsBlocks = 0,
		BuildingBlocks,

		NeedsLight,
		BuildingLight,

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
	std::atomic<bool> isLightBuilt{ false };

	Block blocks[CHUNK_VOLUME];
	uint8_t light[CHUNK_VOLUME];

	std::queue<LightNode> lightQueue;
	mutable std::mutex lightMutex;

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
	void buildLight();
	void buildMesh();

	void updateLight();
	bool hasLightUpdates() const;

	void render() const;
	bool canBeRendered() const;

	Block getBlock_inBoundaries(int x, int y, int z) const;
	Block getBlock_checkSideNeighbor(int x, int y, int z, int side) const;
	Block getBlock_checkNeighborsTraverse(int x, int y, int z) const;

	void setBlock_inBoundaries(int x, int y, int z, Block block);

	uint8_t getLight_inBoundaries(int x, int y, int z) const;
	uint8_t getLight_checkSideNeighbor(int x, int y, int z, int side) const;
	uint8_t getLight_checkNeighborsTraverse(int x, int y, int z) const;

	void setLight_inBoundaries(int x, int y, int z, uint8_t lightValue);

	std::pair<Block, uint8_t> getBlockAndLight_inBoundaries(int x, int y, int z) const;
	std::pair<Block, uint8_t> getBlockAndLight_checkSideNeighbor(int x, int y, int z, int side) const;
	std::pair<Block, uint8_t> getBlockAndLight_checkNeighborsTraverse(int x, int y, int z) const;

	void addNodeToLightQueue(int x, int y, int z, uint8_t lightLevel, int8_t propagationSide);
private:
	void calculateVertexAmbientOcclusionAndLight(int& ao, int& light, uint8_t centerLight, uint8_t side1Light, uint8_t side2Light, uint8_t cornerLight, bool side1Solid, bool side2Solid, bool cornerSolid) const;
	void calculateFaceAmbientOcclusionAndLight(int& ao, int& light, int x, int y, int z, int normal, uint8_t centerFaceLight) const;
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