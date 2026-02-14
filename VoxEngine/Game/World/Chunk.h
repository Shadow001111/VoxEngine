#pragma once
#include "Chunk/ChunkMesh.h"
#include "Chunk/Metrics.h"
#include "Chunk/StructureBlockChanges.h"
#include "Chunk/ChunkSpecializedQueue.h"
#include "Chunk/BufferStreamWriter.h"
#include "Chunk/Light.h"
#include "Chunk/ChunkIO.h"

#include "Core/Multithreading/ProcessingFence.h"
#include "Core/AtomicFlags.h"

//#include "Game/DataPackManagment/DataPackManager.h"

#include "Graphics/DrawCommands.h"

#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <glm/glm.hpp>

struct BlockVertexLightData
{
	unsigned int ao[8];      // AO values for each vertex
	LightLevel light[8];     // Light values for each vertex
};

struct BlockData;

class Chunk
{
	enum Flag : uint8_t
	{
		IsLoadedInWorld = 0,
		IsLoadedChunkColumnData,
		IsMeshDirty
	};
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
	// Chunk coordinates
	glm::ivec3 position;

	// States and flags
	std::atomic<State> state = State::NotInitialized_NeedsBlocks;
	AtomicFlags<uint8_t> chunkFlags;
	bool meshDirty = false;

	// Loaders count
	uint8_t loaderCount = 0;

	// Grid data
	BlockId blocks[CHUNK_VOLUME];
	LightLevel lightLevels[CHUNK_VOLUME];

	// Light propagation
	ChunkSpecializedQueue<LightNode> blockLightBfsQueue;
	mutable std::mutex blockLightBfsMutex;

	ChunkSpecializedQueue<LightRemovalNode> blockLightRemovalBfsQueue;
	mutable std::mutex blockLightRemovalBfsMutex;

	ChunkSpecializedQueue<LightNode> skyLightBfsQueue;
	mutable std::mutex skyLightBfsMutex;
	
	ChunkSpecializedQueue<LightRemovalNode> skyLightRemovalBfsQueue;
	mutable std::mutex skyLightRemovalBfsMutex;

	using LightNodeQueue = ChunkSpecializedQueue<LightNode>;
	using LightRemovalNodeQueue = ChunkSpecializedQueue<LightRemovalNode>;
	static thread_local LightNodeQueue localBlockLightBfsQueue;
	static thread_local LightRemovalNodeQueue localBlockLightRemovalBfsQueue;
	static thread_local LightNodeQueue localSkyLightBfsQueue;
	static thread_local LightRemovalNodeQueue localSkyLightRemovalBfsQueue;

	using LightNodeBulkUpdatesContainer = std::vector<LightNodeBulkUpdate>;
	static thread_local LightNodeBulkUpdatesContainer localBlockLightBulkUpdates[6];
	//static thread_local ChunkSpecializedQueue<LightRemovalNodeBulkUpdate> localBlockLightRemovalBfsQueue;
	static thread_local LightNodeBulkUpdatesContainer localSkyLightBulkUpdates[6];
	//static thread_local ChunkSpecializedQueue<LightRemovalNodeBulkUpdate> localSkyLightRemovalBfsQueue;

	// Mesh
	ChunkMesh mesh;
public:
	static std::atomic<bool> gHasStructureBlockChanges; // TODO: Move this to StructureBlockChangeManager.
private:
	// Processing fence. I tried global processing system. It reduces memory usage because chunk doesn't have its own processing fence.
	// But it increases wait time in average from 4ms to 40ms, trading 1mb for around 7000 chunks. Benefits aren't that big.
	ProcessingFence processingFence;
	
	// Changed blocks
	ChunkIO::BlockChanges changedBlocks;
	
	static StructureBlockChangeManager structureBlockChangeManager;

	// Helper index functions
	static size_t getIndex(int x, int y, int z) { return (x << (CHUNK_SIZE_LOG2 << 1)) | (y << CHUNK_SIZE_LOG2) | z; };
	static glm::ivec3 getPositionFromIndex(size_t index) {
		return {
			(index >> (CHUNK_SIZE_LOG2 << 1)) & CHUNK_LOWER_BITS_MASK,
			(index >> CHUNK_SIZE_LOG2) & CHUNK_LOWER_BITS_MASK,
			index & CHUNK_LOWER_BITS_MASK
		};
	};
public:
	Chunk* neighbors[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }; // Pointers to neighboring chunks for easier access

	// Constructors, destructors, assigments
	Chunk() = default;
	~Chunk();
	Chunk(const Chunk&) = delete;
	Chunk& operator=(const Chunk&) = delete;
	Chunk(Chunk&&) = delete;
	Chunk& operator=(Chunk&&) = delete;

	// Operators
	bool operator==(const Chunk& other) const noexcept { return position == other.position; };

	// Init/destroy
	void init(const glm::ivec3& position, Chunk** neighbors);
	void destroy();
	static void globalInit();

	// Blocks
	void buildBlocks();
	bool hasStructureBlockUpdates() const { return structureBlockChangeManager.hasPendingChanges(position); };
	void updateStructureBlocks();
private:
	void generateTree(const glm::ivec3& position);

	// IO
	void loadBlocks() { ChunkIO::loadBlocks(changedBlocks, position, blocks); }
	void saveBlocks() { ChunkIO::saveBlocks(changedBlocks, position, blocks); }

	// Connectivity
	bool findFloodFillStartIndex(uint16_t& startIndex, const bool* floodFillMask) const;
	void computeConnectivity();

	// Blocks
	void removeIndexFromMap(BlockId block, uint16_t idx);
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

	// Render
	bool canBeRendered() const { return (mesh.faceStorage.alignedCreated || mesh.faceStorage.nonAlignedCreated) && mesh.faceStorage.getRenderFaceCount() > 0; };

	void collectAlignedOpaqueRenderData(BufferStreamWriter<DrawArraysIndirectCommand>& drawCommands, BufferStreamWriter<glm::ivec3>& positions) const;
	void collectAlignedTranslucentRenderData(BufferStreamWriter<DrawArraysIndirectCommand>& drawCommands, BufferStreamWriter<glm::ivec3>& positions) const;
	void collectNonAlignedOpaqueRenderData(BufferStreamWriter<DrawArraysIndirectCommand>& drawCommands, BufferStreamWriter<glm::ivec3>& positions) const;
	void collectNonAlignedTranslucentRenderData(BufferStreamWriter<DrawArraysIndirectCommand>& drawCommands, BufferStreamWriter<glm::ivec3>& positions) const;
private:
	// Chunk traverse
	const Chunk* traverseToSideNeighbor(int x, int y, int z, int side, size_t& outIndex) const;
	Chunk* traverseToSideNeighbor(int x, int y, int z, int side, size_t& outIndex)
	{
		return const_cast<Chunk*>(const_cast<const Chunk*>(this)->traverseToSideNeighbor(x, y, z, side, outIndex)); 
	}

	const Chunk* traverseThroughNeighbors(int x, int y, int z, size_t& outIndex) const;
	Chunk* traverseThroughNeighbors(int x, int y, int z, size_t& outIndex)
	{
		return const_cast<Chunk*>(const_cast<const Chunk*>(this)->traverseThroughNeighbors(x, y, z, outIndex));
	}
public:
	// Grid getters
	BlockId getBlockAt(int x, int y, int z) const;
	LightLevel getLightAt(int x, int y, int z) const;
	std::pair<BlockId, LightLevel> getBlockAndLightAt(int x, int y, int z) const;

	BlockId getBlockAt(size_t index) const;
	LightLevel getLightAt(size_t index) const;
	std::pair<BlockId, LightLevel> getBlockAndLightAt(size_t index) const;

	// Grid setters
	void setBlockAt(int x, int y, int z, BlockId block, bool saveBlockChanges = true);
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

	void applyBlockLightBulkUpdates(const LightNodeBulkUpdatesContainer& bulkUpdates);
	void applySkyLightBulkUpdates(const LightNodeBulkUpdatesContainer& bulkUpdates);
private:
	// Mesh
	void markBlockMeshDirty(int x, int y, int z);

	// Mesh building
	void calculateVertexAmbientOcclusionAndLight(unsigned int& ao, LightLevel& light, LightLevel centerLight, const std::pair<LightLevel, bool>& side1, const std::pair<LightLevel, bool>& side2, const std::pair<LightLevel, bool>& corner) const;
	void calculateFaceAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, int x, int y, int z, int normal, LightLevel centerFaceLight) const;
	
	void calculateBlockVertexLight(BlockVertexLightData& result, int x, int y, int z) const;
public:
	// Chunk data getters
	glm::ivec3 getPosition() const { return position; };
	size_t getFaceCount() const { return mesh.faceStorage.getAllFaceCount(); };

	// Getters and setters for states and flags
	State getState() const { return state.load(std::memory_order_acquire); };
	void setState(State newState);

	bool getIsProcessing() const { return processingFence.isProcessing(); };

	bool getIsLoadedInWorld() const { return chunkFlags.read(Flag::IsLoadedInWorld); };

	bool areBlocksBuilt() const { return getState() >= State::BlocksBuilt; };
	bool isLightBuilt() const { return getState() >= State::LightsBuilt; };

	// Getters and setters for loaderCount
	void addLoader() { loaderCount++; }
	void removeLoader() { loaderCount--; }
	uint8_t getLoaderCount() const { return loaderCount; };
};