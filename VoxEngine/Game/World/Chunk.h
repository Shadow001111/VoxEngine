#pragma once
#include "Chunk/ChunkMesh/ChunkMesh.h"
#include "Chunk/Metrics.h"
#include "Chunk/StructureBlockChanges.h"
#include "Chunk/ChunkSpecializedQueue.h"
#include "Chunk/BufferStreamWriter.h"
#include "Chunk/Light.h"
#include "Chunk/ChunkIO.h"

#include "ChunkRegionManager.h"

#include "Core/Multithreading/ProcessingFence.h"
#include "Core/AtomicFlags.h"

//#include "Game/DataPackManagment/DataPackManager.h"

#include "Graphics/DrawCommands.h"

#include <mutex>
#include <atomic>
#include <cstdint>
#include <array>
#include <glm/glm.hpp>

constexpr auto precomputeNeighborDirtyMasks()
{
	std::array<uint32_t, 27> lut{};
	for (int dx = -1; dx <= 1; ++dx)
		for (int dy = -1; dy <= 1; ++dy)
			for (int dz = -1; dz <= 1; ++dz)
			{
				uint32_t mask = 0;
				for (int nx = -1; nx <= 1; ++nx)
					for (int ny = -1; ny <= 1; ++ny)
						for (int nz = -1; nz <= 1; ++nz)
						{
							// For a block that is at offset (dx,dy,dz) from chunk center (0,0,0),
							// which neighbors can it affect? The block is at a position that is:
							// - If dx = -1: block is at x=0 (negative border)
							// - If dx = 0: block is interior (1..CHUNK_SIZE-2)
							// - If dx = 1: block is at x=CHUNK_SIZE-1 (positive border)

							// The neighbor at offset (nx,ny,nz) should be marked dirty if:
							// The block is on the border facing that neighbor
							bool x = (nx == 0) || (nx == -1 && dx == -1) || (nx == 1 && dx == 1);
							bool y = (ny == 0) || (ny == -1 && dy == -1) || (ny == 1 && dy == 1);
							bool z = (nz == 0) || (nz == -1 && dz == -1) || (nz == 1 && dz == 1);

							if (x && y && z)
							{
								int index = (nx + 1) * 9 + (ny + 1) * 3 + (nz + 1);
								mask |= 1u << index;
							}
						}
				int lutIndex = (dx + 1) * 9 + (dy + 1) * 3 + (dz + 1);
				lut[lutIndex] = mask;
			}
	return lut;
}

struct BlockVertexLightData
{
	unsigned int ao[8];      // AO values for each vertex
	LightLevel light[8];     // Light values for each vertex
};

struct BlockData;

class Chunk
{
	struct DirectionsTable
	{
		std::array<int, 6> dx{ -1, 1, 0, 0, 0, 0 };
		std::array<int, 6> dy{ 0, 0, -1, 1, 0, 0 };
		std::array<int, 6> dz{ 0, 0, 0, 0, -1, 1 };
	};

	struct CachedBlockIds
	{
		BlockId airId;
		BlockId waterId;
		BlockId grassBlockId;
		BlockId dirtId;
		BlockId stoneId;
		BlockId oakLogId;
		BlockId oakLeavesId;
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

	static std::atomic<bool> gHasStructureBlockChanges; // TODO: Move this to StructureBlockChangeManager.
	static ChunkRegionManager chunkRegionManagerInstance;

	static constexpr std::array<uint32_t, 27> PRECOMPUTED_NEIGHBOR_DIRTY_MASKS = precomputeNeighborDirtyMasks();
	static const DirectionsTable DIRECTIONS_TABLE;
	static CachedBlockIds CACHED_BLOCK_IDS;
private:
	enum Flag : uint8_t
	{
		IsLoadedInWorld = 0,
		IsLoadedChunkColumnData,
		ShouldUpdateMesh
	};

	// Chunk coordinates
	glm::ivec3 position;

	// States and flags
	std::atomic<State> state = State::NotInitialized_NeedsBlocks;
	AtomicFlags<uint8_t> chunkFlags;

	// Loaders count
	uint8_t loaderCount = 0;

	// Processing fence
	AtomicWaitFence processingFence;

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

	static thread_local ChunkSpecializedQueue<LightNode> localBlockLightBfsQueue;
	static thread_local ChunkSpecializedQueue<LightRemovalNode> localBlockLightRemovalBfsQueue;
	static thread_local ChunkSpecializedQueue<LightNode> localSkyLightBfsQueue;
	static thread_local ChunkSpecializedQueue<LightRemovalNode> localSkyLightRemovalBfsQueue;

	// Mesh
	ChunkMesh mesh;

	static thread_local ChunkInstancedMeshFaceStorage::InstancesStorage localMeshInstances;
	
	// Changed blocks
	ChunkIO::BlockChanges changedBlocks;
	
	static StructureBlockChangeManager structureBlockChangeManager;

	// Helper index functions
	static size_t getIndex(int x, int y, int z) noexcept { return (x << (CHUNK_SIZE_LOG2 << 1)) | (y << CHUNK_SIZE_LOG2) | z; };
	static size_t getIndex(int x, int z) noexcept { return (x << CHUNK_SIZE_LOG2) | z; };

	static glm::ivec3 getPositionFromIndex(int index) noexcept // Took 'size_t', but now takes 'int' because think it will be cheaper (less casts)
	{
		return {
			(index >> (CHUNK_SIZE_LOG2 << 1)) & CHUNK_LOWER_BITS_MASK,
			(index >> CHUNK_SIZE_LOG2) & CHUNK_LOWER_BITS_MASK,
			index & CHUNK_LOWER_BITS_MASK
		};
	};

	std::array<Chunk*, 27> neighbors{ nullptr }; // Pointers to neighboring chunks for easier access. Could store 26, but storing 27 allows to use easier indexing.
	ChunkRegion* parentRegion = nullptr; // Pointer to the parent region, set when chunk is added to a region
public:
	static constexpr inline int getNeighborIndex(int dx, int dy, int dz) noexcept
	{
		return (dx + 1) * 9 + (dy + 1) * 3 + (dz + 1);
	}

	static constexpr inline int getOppositeNeighborIndex(int idx) noexcept
	{
		return 26 - idx;
	}

	static constexpr inline glm::ivec3 getNeighborOffset(int idx) noexcept
	{
		return { (idx / 9) - 1, (idx / 3 % 3) - 1, (idx % 3) - 1 };
	}

	static constexpr inline int getSideNeighborIndex(int side) noexcept
	{
		static constexpr std::array<int, 6> sideToNeighborIndex = {
			getNeighborIndex(-1, 0, 0), // Left
			getNeighborIndex(1, 0, 0),  // Right
			getNeighborIndex(0, -1, 0), // Down
			getNeighborIndex(0, 1, 0),  // Up
			getNeighborIndex(0, 0, -1), // Back
			getNeighborIndex(0, 0, 1)   // Front
		};
		return sideToNeighborIndex[side];
	}

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
	void init(const glm::ivec3& position, const std::array<Chunk*, 27>& newNeighbors, ChunkRegion* parentRegion);
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

	// Light
	void propagateBlockLight(uint32_t& neighborDirtyMask);
	void propagateSkyLight(uint32_t& neighborDirtyMask);
	void propagateBlockLightRemoval(uint32_t& neighborDirtyMask);
	void propagateSkyLightRemoval(uint32_t& neighborDirtyMask);
public:
	// Light
	void buildLight();
	void updateLight();
	bool hasLightUpdates() const noexcept
	{
		return
			blockLightBfsQueue.size() ||
			blockLightRemovalBfsQueue.size() ||
			skyLightBfsQueue.size() ||
			skyLightRemovalBfsQueue.size();
	}

	// Mesh
	void updateMesh();
	bool shouldMeshBeUpdated() const { return chunkFlags.read(Flag::ShouldUpdateMesh) && isLightBuilt(); };
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
	Chunk* traverseToSideNeighbor(int x, int y, int z, int side, size_t& outIndex) const;
	Chunk* traverseThroughNeighbors(int x, int y, int z, size_t& outIndex) const;
public:
	//
	static uint32_t getNeighborDirtyMask(int x, int y, int z) noexcept;
	void applyNeighborDirtyMask(uint32_t mask);

	// Grid getters
	BlockId getBlockAt(int x, int y, int z) const { return blocks[getIndex(x, y, z)]; }
	LightLevel getLightAt(int x, int y, int z) const { return lightLevels[getIndex(x, y, z)]; }
	std::pair<BlockId, LightLevel> getBlockAndLightAt(int x, int y, int z) const;

	BlockId getBlockAt(size_t index) const { return blocks[index]; }
	LightLevel getLightAt(size_t index) const { return lightLevels[index]; }
	std::pair<BlockId, LightLevel> getBlockAndLightAt(size_t index) const { return std::make_pair(blocks[index], lightLevels[index]); }

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
private:
	// Mesh
	void markMeshesDirtyAroundBlock(int x, int y, int z);

	// Mesh building
	void calculateVertexAmbientOcclusionAndLight(unsigned int& ao, LightLevel& light, LightLevel centerLight, const std::pair<LightLevel, bool>& side1, const std::pair<LightLevel, bool>& side2, const std::pair<LightLevel, bool>& corner) const;
	void calculateFaceAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, int x, int y, int z, int normal, LightLevel centerFaceLight) const;
	
	void calculateBlockVertexLight(BlockVertexLightData& result, int x, int y, int z) const;
public:
	// Chunk data getters
	glm::ivec3 getPosition() const noexcept { return position; };
	const auto& getNeighbors() const noexcept { return neighbors; };
	size_t getFaceCount() const noexcept { return mesh.faceStorage.getAllFaceCount(); };

	// Getters and setters for states and flags
	State getState() const noexcept { return state.load(std::memory_order_acquire); };
	void setState(State newState) noexcept { state.store(newState, std::memory_order_release); }

	void setFlag(Flag flag, bool value) noexcept { chunkFlags.set(static_cast<unsigned>(flag), value); }
	bool readFlag(Flag flag) const noexcept { return chunkFlags.read(static_cast<unsigned>(flag)); }

	bool getIsProcessing() const noexcept { return processingFence.isProcessing(); };

	bool getIsLoadedInWorld() const noexcept { return chunkFlags.read(Flag::IsLoadedInWorld); };

	bool areBlocksBuilt() const noexcept { return getState() >= State::BlocksBuilt; };
	bool isLightBuilt() const noexcept { return getState() >= State::LightsBuilt; };

	// Getters and setters for loaderCount
	void addLoader() noexcept { loaderCount++; }
	void removeLoader() noexcept { loaderCount--; }
	uint8_t getLoaderCount() const noexcept { return loaderCount; };
};