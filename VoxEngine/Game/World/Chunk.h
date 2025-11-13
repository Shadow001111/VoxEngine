#pragma once
#include "Chunk/MeshData.h"
#include "Chunk/Block.h"
#include "Chunk/Metrics.h"

#include "Core/Multithreading/ProcessingFence.h"
#include "Core/AtomicFlags.h"

#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <glm/glm.hpp>
#include <filesystem>
#include <unordered_map>

// Forward declarations

//
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
		NotInitialized_NeedsBlocks = 0,
		BuildingBlocks,
		BlocksBuilt,

		NeedsLight,
		BuildingLight,
		LightsBuilt
	};
private:
	enum Flag : uint8_t
	{
		IsLoadedInWorld = 0,
		IsLoadedChunkColumnData
	};
	
	// Chunk coordinates
	glm::ivec3 position;

	//
	uint16_t cameraClosestBlockPosForSortingMesh; // 5 bits per axis

	// States and flags
	std::atomic<State> state;
	AtomicFlags<uint8_t> chunkFlags;
	bool meshDirty;
	bool shouldSortMeshAfterBuild;

	// Loaders count
	uint8_t loaderCount;

	// Grid data
	Block blocks[CHUNK_VOLUME];
	LightLevel lightLevels[CHUNK_VOLUME];

	//  Light propagation
	std::queue<LightNode> blockLightBfsQueue;
	mutable std::mutex blockLightBfsMutex;

	std::queue<LightRemovalNode> blockLightRemovalBfsQueue;
	mutable std::mutex blockLightRemovalBfsMutex;

	std::queue<LightNode> skyLightBfsQueue;
	mutable std::mutex skyLightBfsMutex;

	std::queue<LightRemovalNode> skyLightRemovalBfsQueue;
	mutable std::mutex skyLightRemovalBfsMutex;

	// Mesh
	MeshData meshData;
	static std::vector<MeshData*> pendingMeshUploads;

	// Processing fence
	ProcessingFence processingFence;
	
	// Changed blocks
	std::unordered_map<Block, std::vector<uint16_t>> changedBlocks;

	// Helper index functions
	static size_t getIndex(int x, int y, int z);
	static glm::ivec3 getPositionFromIndex(size_t index);
public:
	Chunk* neighbors[6]; // Pointers to neighboring chunks for easier access
	
	static std::filesystem::path WORLD_PATH;

	// Constructors, destructors, assigments
	Chunk();
	~Chunk();
	Chunk(const Chunk&) = delete;
	Chunk& operator=(const Chunk&) = delete;
	Chunk(Chunk&&) = delete;
	Chunk& operator=(Chunk&&) = delete;

	// Operators
	bool operator==(const Chunk& other) const;

	//
	void init(const glm::ivec3& position, Chunk** neighbors);
	void destroy();

	void buildBlocks();
private:
	void generateTree(const glm::ivec3& position);
private:
	// IO
	void loadBlocks();
	void saveBlocks() const;
private:
	// Connectivity
	bool findFloodFillStartIndex(uint16_t& startIndex, const bool* floodFillMask) const;
	void computeConnectivity();
	void removeIndexFromMap(Block block, uint16_t idx);
public:
	void buildLight();
	void updateLight();
	bool hasLightUpdates() const;

	// Mesh
	void updateMesh();
	void sortMesh(const glm::ivec3& cameraBlockPos);
	bool shouldMeshBeSorted(bool cameraMoved) const;
	bool shouldMeshBeUpdated() const;
	void markMeshDirty();
	void askForMeshUpload();
	static void sendMeshesToGPU();

	// Render
	void collectOpaqueRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	void collectTransparentRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	bool canBeRendered() const;

	// Chunk traverse
	const Chunk* getChunkAndIndex_checkSideNeighbor(int x, int y, int z, int side, size_t& outIndex) const;
	Chunk* getChunkAndIndex_checkSideNeighbor(int x, int y, int z, int side, size_t& outIndex);

	const Chunk* getChunkAndIndex_checkNeighborsTraverse(int x, int y, int z, size_t& outIndex) const;
	Chunk* getChunkAndIndex_checkNeighborsTraverse(int x, int y, int z, size_t& outIndex);

	// Grid getters
	Block getBlockAt(int x, int y, int z) const;
	LightLevel getLightAt(int x, int y, int z) const;
	std::pair<Block, LightLevel> getBlockAndLightAt(int x, int y, int z) const;

	Block getBlockAt(size_t index) const;
	LightLevel getLightAt(size_t index) const;
	std::pair<Block, LightLevel> getBlockAndLightAt(size_t index) const;

	// Grid setters
	void setBlockAt(int x, int y, int z, Block block);
	void setLightAt(int x, int y, int z, LightLevel lightLevel);
	void setBlockLightAt(int x, int y, int z, uint8_t lightLevel);
	void setSkyLightAt(int x, int y, int z, uint8_t lightLevel);

	void setLightAt(size_t index, LightLevel lightValue);
	void setBlockLightAt(size_t index, uint8_t lightLevel);
	void setSkyLightAt(size_t index, uint8_t lightLevel);

	// Light propagation
	void addBlockLightNodeToQueue(int x, int y, int z);
	void addBlockLightRemovalNodeToQueue(int x, int y, int z, uint8_t lightLevel);
	void addSkyLightNodeToQueue(int x, int y, int z);
	void addSkyLightRemovalNodeToQueue(int x, int y, int z, uint8_t lightLevel);
private:
	// Mesh?
	void markBlockMeshDirty(int x, int y, int z);
private:
	// Mesh building
	void calculateVertexAmbientOcclusionAndLight(unsigned int& ao, LightLevel& light, LightLevel centerLight, LightLevel side1Light, LightLevel side2Light, LightLevel cornerLight, bool side1Solid, bool side2Solid, bool cornerSolid) const;
	void calculateFaceAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, int x, int y, int z, int normal, LightLevel centerFaceLight) const;
public:
	// Chunk data getters
	glm::ivec3 getPosition() const;
	size_t getFaceCount() const;
	size_t getFaceCapacity() const;

	// Getters and setters for states and flags
	State getState() const;
	void setState(State newState);

	bool getIsProcessing() const;

	bool getIsLoadedInWorld() const;

	bool areBlocksBuilt() const;
	bool isLightBuilt() const;

	// Getters and setters for loaderCount
	void addLoader();
	void removeLoader();
	uint8_t getLoaderCount() const;
};