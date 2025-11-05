#pragma once
#include "Chunk/MeshData.h"
#include "Chunk/Block.h"
#include "Chunk/Metrics.h"

#include "Core/Multithreading/ProcessingFence.h"

#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <glm/glm.hpp>
#include <bitset>

struct LightLevel
{
	uint8_t blockLight : 4;
	uint8_t skyLight : 4;

	LightLevel();
	LightLevel(uint8_t blockLight, uint8_t skyLight);

	LightLevel(const LightLevel& other);

	LightLevel& operator=(const LightLevel& other);
};

struct LightNode
{
	uint16_t x : 4, y : 4, z : 4;

	LightNode(int x, int y, int z);
};

struct LightRemovalNode
{
	uint16_t x : 4, y : 4, z : 4, lightLevel : 4;

	LightRemovalNode(int x, int y, int z, uint8_t lightLevel);
};

struct DrawArraysIndirectCommand
{
	unsigned int count;        // Number of vertices per instance
	unsigned int instanceCount;// Number of instances to draw
	unsigned int first;        // Starting vertex index in the vertex array
	unsigned int baseInstance; // Base instance ID

	DrawArraysIndirectCommand(unsigned int count, unsigned int instanceCount, unsigned int first, unsigned int baseInstance);
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
	glm::ivec3 position; // Chunk coordinates

	uint16_t cameraClosestBlockPosForSortingMesh; // 5 bits per axis

	std::atomic<State> state;
	// TODO: Maybe use atomic bitset?
	std::atomic<bool> isLoadedInWorld{ false };
	std::atomic<bool> isLoadedChunkColumnData { false };
	std::atomic<bool> areBlocksBuilt{ false };
	std::atomic<bool> isLightBuilt{ false };
	bool meshDirty;

	bool shouldSortMeshAfterBuild;

	Block blocks[CHUNK_VOLUME];
	LightLevel lightLevels[CHUNK_VOLUME];

	std::queue<LightNode> blockLightBfsQueue;
	mutable std::mutex blockLightBfsMutex;

	std::queue<LightRemovalNode> blockLightRemovalBfsQueue;
	mutable std::mutex blockLightRemovalBfsMutex;

	std::queue<LightNode> skyLightBfsQueue;
	mutable std::mutex skyLightBfsMutex;

	std::queue<LightRemovalNode> skyLightRemovalBfsQueue;
	mutable std::mutex skyLightRemovalBfsMutex;

	MeshData meshData;
	ProcessingFence processingFence;

	static std::vector<MeshData*> pendingMeshUploads;

	static size_t getIndex(int x, int y, int z);
	static glm::ivec3 getPositionFromIndex(size_t index);
public:
	Chunk* neighbors[6]; // Pointers to neighboring chunks for easier access

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
private:
	bool findFloodFillStartIndex(uint16_t& startIndex, const bool* floodFillMask) const;
	void computeConnectivity();
public:
	void buildLight();
	void updateLight();
	bool hasLightUpdates() const;

	void updateMesh();

	void sortMesh(const glm::ivec3& cameraBlockPos);
	bool shouldMeshBeSorted(bool cameraMoved) const;
	bool isMeshDirty() const;
	void markMeshDirty();
	void askForMeshUpload();

	void collectOpaqueRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	void collectTransparentRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	bool canBeRendered() const;

	const Chunk* getChunkAndIndex_checkSideNeighbor(int x, int y, int z, int side, size_t& outIndex) const;
	Chunk* getChunkAndIndex_checkSideNeighbor(int x, int y, int z, int side, size_t& outIndex);

	const Chunk* getChunkAndIndex_checkNeighborsTraverse(int x, int y, int z, size_t& outIndex) const;
	Chunk* getChunkAndIndex_checkNeighborsTraverse(int x, int y, int z, size_t& outIndex);

	Block getBlockAt(int x, int y, int z) const;
	LightLevel getLightAt(int x, int y, int z) const;
	std::pair<Block, LightLevel> getBlockAndLightAt(int x, int y, int z) const;

	Block getBlockAt(size_t index) const;
	LightLevel getLightAt(size_t index) const;
	std::pair<Block, LightLevel> getBlockAndLightAt(size_t index) const;

	void setBlockAt(int x, int y, int z, Block block);
	void setLightAt(int x, int y, int z, LightLevel lightLevel);
	void setBlockLightAt(int x, int y, int z, uint8_t lightLevel);
	void setSkyLightAt(int x, int y, int z, uint8_t lightLevel);

	void setBlockAt(size_t index, Block block);
	//void setBlockAt_update(size_t index, Block block);
	void setLightAt(size_t index, LightLevel lightValue);
	void setBlockLightAt(size_t index, uint8_t lightLevel);
	void setSkyLightAt(size_t index, uint8_t lightLevel);

	void addBlockLightNodeToQueue(int x, int y, int z);
	void addBlockLightRemovalNodeToQueue(int x, int y, int z, uint8_t lightLevel);
	void addSkyLightNodeToQueue(int x, int y, int z);
	void addSkyLightRemovalNodeToQueue(int x, int y, int z, uint8_t lightLevel);
private:
	void markBlockMeshDirty(int x, int y, int z);
private:
	void calculateVertexAmbientOcclusionAndLight(unsigned int& ao, LightLevel& light, LightLevel centerLight, LightLevel side1Light, LightLevel side2Light, LightLevel cornerLight, bool side1Solid, bool side2Solid, bool cornerSolid) const;
	void calculateFaceAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, int x, int y, int z, int normal, LightLevel centerFaceLight) const;
public:
	int getX() const;
	int getY() const;
	int getZ() const;
	glm::ivec3 getPosition() const;
	size_t getFaceCount() const;
	size_t getFaceCapacity() const;

	// Atomic getters and setters
	State getState() const;
	void setState(State newState);

	bool getIsProcessing() const;

	bool getIsLoadedInWorld() const;

	//
	static void sendMeshesToGPU();
};


struct Int3Hasher
{
public:
	size_t operator()(const glm::ivec3& other) const;
};