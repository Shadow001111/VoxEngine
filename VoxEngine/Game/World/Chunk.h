#pragma once
#include "Chunk/MeshData.h"
#include "Chunk/BlockData.h"
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
	LightLevel(LightLevel&& other);

	LightLevel& operator=(const LightLevel& other);
	LightLevel& operator=(LightLevel&& other) noexcept;
};

// TODO: Maybe use union to use position as index
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
	glm::ivec3 position; // Chunk coordinates in chunk space

	uint16_t cameraClosestBlockPosForSortingMesh; // 5 bits per axis

	std::atomic<State> state;
	std::atomic<bool> isLoadedInWorld{ false };
	std::atomic<bool> isLoadedChunkColumnData { false };
	std::atomic<bool> areBlocksBuilt{ false };
	std::atomic<bool> isLightBuilt{ false };

	bool shouldSortMeshAfterBuild;

	Block blocks[CHUNK_VOLUME];
	LightLevel lightLevels[CHUNK_VOLUME];

	std::queue<LightNode> lightNodeContainer;
	mutable std::mutex lightNodeMutex;

	std::queue<LightRemovalNode> lightRemovalNodeContainer;
	mutable std::mutex lightRemovalNodeMutex;

	MeshData meshData;
	ProcessingFence processingFence;

	static std::vector<MeshData*> pendingMeshUploads;

	static size_t getIndex(int x, int y, int z);
	static glm::ivec3 getPositionFromIndex(size_t index);
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
private:
	bool findFloodFillStartIndex(uint16_t& startIndex, const bool* floodFillMask) const;
	void computeConnectivity();
public:
	void buildLight();
	void buildMesh();

	std::bitset<27> updateLight();
	bool hasLightUpdates() const;

	void sortMesh(const glm::ivec3& cameraBlockPos);
	bool shouldMeshBeSorted(bool cameraMoved) const;
	void askForMeshUpload();

	void collectOpaqueRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	void collectTransparentRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	bool canBeRendered() const;

	Block getBlock_inBoundaries(int x, int y, int z) const;
	Block getBlock_checkSideNeighbor(int x, int y, int z, int side) const;
	Block getBlock_checkNeighborsTraverse(int x, int y, int z) const;

	void setBlock_inBoundaries(int x, int y, int z, Block block);
	void setBlock_inBoundaries_updateLight(int x, int y, int z, Block block);

	LightLevel getLight_inBoundaries(int x, int y, int z) const;
	LightLevel getLight_checkSideNeighbor(int x, int y, int z, int side) const;
	LightLevel getLight_checkNeighborsTraverse(int x, int y, int z) const;

	void setLight_inBoundaries(int x, int y, int z, LightLevel lightValue);

	std::pair<Block, LightLevel> getBlockAndLight_inBoundaries(int x, int y, int z) const;
	std::pair<Block, LightLevel> getBlockAndLight_checkSideNeighbor(int x, int y, int z, int side) const;
	std::pair<Block, LightLevel> getBlockAndLight_checkNeighborsTraverse(int x, int y, int z) const;

	void addLightNodeToQueue(int x, int y, int z);
	void addLightRemovalNodeToQueue(int x, int y, int z, uint8_t lightLevel);
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