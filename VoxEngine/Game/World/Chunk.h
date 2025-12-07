#pragma once
#include "Chunk/MeshData.h"
#include "Chunk/Block.h"
#include "Chunk/Metrics.h"
#include "Chunk/StructureBlockChanges.h"
#include "Chunk/ChunkSpecializedQueue.h"

#include "Core/Multithreading/ProcessingFence.h"
#include "Core/AtomicFlags.h"

#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <glm/glm.hpp>
#include <filesystem>
#include <unordered_map>

union LightLevel
{
	struct
	{
		uint8_t blockLight : 4;
		uint8_t skyLight : 4;
	};

	uint8_t fullByte;

	LightLevel();
	LightLevel(uint8_t blockLight, uint8_t skyLight);

	LightLevel(const LightLevel& other);
	LightLevel& operator=(const LightLevel& other);
};

struct LightNode
{
	uint8_t x : 4, y : 4, z : 4;

	LightNode(int x, int y, int z);
};

struct LightRemovalNode
{
	uint8_t x : 4, y : 4, z : 4, lightLevel : 4;

	LightRemovalNode(int x, int y, int z, uint8_t lightLevel);
};

struct DrawArraysIndirectCommand
{
	unsigned int count;        // Number of vertices per instance
	unsigned int instanceCount;// Number of instances to draw
	unsigned int first;        // Starting vertex index in the vertex array
	unsigned int baseInstance; // Base instance ID

	DrawArraysIndirectCommand() = default;
	DrawArraysIndirectCommand(unsigned int count, unsigned int instanceCount, unsigned int first, unsigned int baseInstance);
};

struct BlockVertexLightData
{
	unsigned int ao[8];      // AO values for each vertex
	LightLevel light[8];     // Light values for each vertex
};

// TODO: Keep often-acessed data closer
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

	// States and flags
	std::atomic<State> state = State::NotInitialized_NeedsBlocks;
	AtomicFlags<uint8_t> chunkFlags;
	bool meshDirty = false;

	// Loaders count
	uint8_t loaderCount = 0;

	// Grid data
	BlockID blocks[CHUNK_VOLUME];
	LightLevel lightLevels[CHUNK_VOLUME];

	//  Light propagation
	ChunkSpecializedQueue<LightNode> blockLightBfsQueue;
	mutable std::mutex blockLightBfsMutex;

	ChunkSpecializedQueue<LightRemovalNode> blockLightRemovalBfsQueue;
	mutable std::mutex blockLightRemovalBfsMutex;

	ChunkSpecializedQueue<LightNode> skyLightBfsQueue;
	mutable std::mutex skyLightBfsMutex;
	
	ChunkSpecializedQueue<LightRemovalNode> skyLightRemovalBfsQueue;
	mutable std::mutex skyLightRemovalBfsMutex;

	// Mesh
	ChunkMeshData meshData;
	static std::vector<ChunkMeshData*> pendingMeshUploads;

	// Processing fence
	// TODO: Maybe instead of having one fence per chunk, have some sort of global processing system?
	// Chunk should check if unorderep_set contains chunk position. If yes, wait until it's removed.
	// It should reduce number of fences significantly. One fence per thread, not per chunk. Reduces memory usage.
	ProcessingFence processingFence;
	
	// Changed blocks
	std::unordered_map<BlockID, std::vector<uint16_t>> changedBlocks;
	
	static StructureBlockChangeManager structureBlockChangeManager;

	// Helper index functions
	static size_t getIndex(int x, int y, int z) { return (x << (CHUNK_SIZE_LOG2 << 1)) | (y << CHUNK_SIZE_LOG2) | z; };
	static glm::ivec3 getPositionFromIndex(size_t index);
public:
	Chunk* neighbors[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }; // Pointers to neighboring chunks for easier access
	
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

	// Init/destroy
	void init(const glm::ivec3& position, Chunk** neighbors);
	void destroy();

	// Blocks
	void buildBlocks();
	bool hasStructureBlockUpdates() const;
	void updateStructureBlocks();
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

	// Blocks
	void removeIndexFromMap(BlockID block, uint16_t idx);
public:
	// Light
	void buildLight();
	void updateLight();
	bool hasLightUpdates() const;

	// Mesh
	void updateMesh();
	bool shouldMeshBeUpdated() const { return meshDirty&& isLightBuilt(); };
	void markMeshDirty();
	void askForMeshUpload();
	static void sendMeshesToGPU();

	// Render
	void collectAlignedOpaqueRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	void collectAlignedTranslucentRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	void collectNonAlignedOpaqueRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	void collectNonAlignedTranslucentRenderData(std::vector<DrawArraysIndirectCommand>& drawCommands, std::vector<glm::ivec3>& positions) const;
	bool canBeRendered() const;

	// Chunk traverse
private:
	const Chunk* getChunkAndIndex_checkSideNeighbor(int x, int y, int z, int side, size_t& outIndex) const;
	Chunk* getChunkAndIndex_checkSideNeighbor(int x, int y, int z, int side, size_t& outIndex);

	const Chunk* getChunkAndIndex_checkNeighborsTraverse(int x, int y, int z, size_t& outIndex) const;
	Chunk* getChunkAndIndex_checkNeighborsTraverse(int x, int y, int z, size_t& outIndex);
public:
	// Grid getters
	BlockID getBlockAt(int x, int y, int z) const;
	LightLevel getLightAt(int x, int y, int z) const;
	std::pair<BlockID, LightLevel> getBlockAndLightAt(int x, int y, int z) const;

	BlockID getBlockAt(size_t index) const;
	LightLevel getLightAt(size_t index) const;
	std::pair<BlockID, LightLevel> getBlockAndLightAt(size_t index) const;

	// Grid setters
	void setBlockAt(int x, int y, int z, BlockID block, bool saveBlockChanges = true);
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
	void calculateVertexAmbientOcclusionAndLight(unsigned int& ao, LightLevel& light, LightLevel centerLight, const std::pair<LightLevel, bool>& side1, const std::pair<LightLevel, bool>& side2, const std::pair<LightLevel, bool>& corner) const;
	void calculateFaceAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, int x, int y, int z, int normal, LightLevel centerFaceLight) const;
	
	void calculateBlockVertexLight(BlockVertexLightData& result, int x, int y, int z) const;
public:
	// Chunk data getters
	glm::ivec3 getPosition() const { return position; };
	size_t getFaceCount() const { return meshData.getAllFaceCount(); };

	// Getters and setters for states and flags
	State getState() const { return state.load(std::memory_order_acquire); };
	void setState(State newState);

	bool getIsProcessing() const { return processingFence.isProcessing(); };

	bool getIsLoadedInWorld() const { return chunkFlags.read(Flag::IsLoadedInWorld); };

	bool areBlocksBuilt() const { return getState() >= State::BlocksBuilt; };
	bool isLightBuilt() const { return getState() >= State::LightsBuilt; };

	// Getters and setters for loaderCount
	void addLoader();
	void removeLoader();
	uint8_t getLoaderCount() const { return loaderCount; };
};