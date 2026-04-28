#pragma once
#include "ChunkRegionManager.h"

#include "Chunk/ChunkMesh/ChunkMesh.h"
#include "Chunk/Metrics.h"
#include "Chunk/StructureBlockChanges.h"
#include "Chunk/BufferStreamWriter.h"
#include "Chunk/Light.h"
#include "Chunk/ChunkIO.h"

#include "Core/Multithreading/ProcessingFence.h"
#include "Core/AtomicFlags.h"
#include "Core/SymmetricBitMatrix.h"
#include "Core/Assert.h"

#include "Graphics/DrawCommands.h"

#include <atomic>
#include <cstdint>
#include <array>
#include <glm/glm.hpp>

namespace detail
{
	constexpr auto precomputeNeighborDirtyMasks()
	{
		std::array<uint32_t, 27> lut{};
		for (int dx = -1; dx <= 1; dx++)
		for (int dy = -1; dy <= 1; dy++)
		for (int dz = -1; dz <= 1; dz++)
		{
			uint32_t mask = 0;
			for (int nx = -1; nx <= 1; nx++)
			for (int ny = -1; ny <= 1; ny++)
			for (int nz = -1; nz <= 1; nz++)
			{
				// For a block that is at offset (dx,dy,dz) from chunk center (0,0,0),
				// which neighbors can it affect? The block is at a position that is:
				// - If dx = -1: block is at x=0 (negative border)
				// - If dx =  0: block is interior (1..CHUNK_SIZE-2)
				// - If dx =  1: block is at x=CHUNK_SIZE-1 (positive border)

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

	static constexpr glm::ivec3 computeNeighborOffset(int idx) noexcept
	{
		return { (idx / 9) - 1, (idx / 3 % 3) - 1, (idx % 3) - 1 };
	}

	static constexpr auto buildNeighborOffsetTable() noexcept
	{
		std::array<glm::ivec3, 27> table{};
		for (int i = 0; i < 27; i++)
			table[i] = computeNeighborOffset(i);
		return table;
	}
}

struct DirectionsTable
{
	static constexpr std::array<int, 6> dx{ -1, 1, 0, 0, 0, 0 };
	static constexpr std::array<int, 6> dy{ 0, 0, -1, 1, 0, 0 };
	static constexpr std::array<int, 6> dz{ 0, 0, 0, 0, -1, 1 };

	static constexpr std::array<glm::ivec3, 6> directionsXYZ{ {
		{-1,  0,  0},
		{ 1,  0,  0},
		{ 0, -1,  0},
		{ 0,  1,  0},
		{ 0,  0, -1},
		{ 0,  0,  1},
	} };

	static constexpr std::array<glm::ivec3, 6> directionsXZY{ {
		{-1,  0,  0},
		{ 1,  0,  0},
		{ 0,  0, -1},
		{ 0,  0,  1},
		{ 0, -1,  0},
		{ 0,  1,  0},
	} };
};

struct BlockData;

class Chunk
{
	// Types
	struct CoordinatesStride3D
	{
		static constexpr int x = CHUNK_AREA;
		static constexpr int y = CHUNK_SIZE;
		static constexpr int z = 1;
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

	struct LightLevelAndIsSolid
	{
		LightLevel lightLevel;
		bool isSolid = true;
	};

	struct BlockVertexData // TODO: Find a better name
	{
		// AO and Light data for 24 vertices (8 would be perfect, but due to same vertices having same values it's just a wet dream)
		uint8_t ao[6];
		LightLevel light[24];
	};

	struct Cell
	{
		BlockId block;
		LightLevel lightLevel;
	};

	// Types
public:
	enum class State : uint8_t
	{
		NotInitialized_NeedsBlocks = 0,
		BuildingBlocks,

		BlocksBuit_NeedsLight,
		BuildingLight,

		LightsBuilt
	};

	enum Flag : uint8_t
	{
		IsLoadedInWorld = 0,
		IsLoadedChunkColumnData,
		ShouldUpdateMesh,
		//ShouldUpdateLight,
		CanBeRendered,
		ShouldUpdateConnectivity
	};

	struct GlobalCounters
	{
		std::atomic<uint32_t> chunkCount{ 0 };
		std::atomic<int64_t> faceCount{ 0 };
	};
	
	// Static helpers
	static size_t getIndex(int x, int y, int z) noexcept { return (x << (CHUNK_SIZE_LOG2 << 1)) | (y << CHUNK_SIZE_LOG2) | z; };

	static size_t getIndex(const glm::ivec3& pos) noexcept { return (pos.x << (CHUNK_SIZE_LOG2 << 1)) | (pos.y << CHUNK_SIZE_LOG2) | pos.z; };

	static size_t getIndex(int x, int y) noexcept { return (x << CHUNK_SIZE_LOG2) | y; };

	static size_t getIndex(const glm::ivec2& pos) noexcept { return (pos.x << CHUNK_SIZE_LOG2) | pos.y; };

	static constexpr int getNeighborIndex(int dx, int dy, int dz) noexcept
	{
		return (dx + 1) * 9 + (dy + 1) * 3 + (dz + 1);
	}

	static int getOppositeNeighborIndex(int idx) noexcept
	{
		return 26 - idx;
	}

	static glm::ivec3 getNeighborOffset(int idx) noexcept
	{
		constexpr auto table = detail::buildNeighborOffsetTable();
		return table[idx];
	}

	static int getSideNeighborIndex(int side) noexcept
	{
		switch (side)
		{
		case 0: return getNeighborIndex(-1, 0, 0);
		case 1: return getNeighborIndex(1, 0, 0);
		case 2: return getNeighborIndex(0, -1, 0);
		case 3: return getNeighborIndex(0, 1, 0);
		case 4: return getNeighborIndex(0, 0, -1);
		case 5: return getNeighborIndex(0, 0, 1);
		}
		return 0;
	}

	static glm::ivec3 getPositionFromIndex(int index) noexcept
	{
		return {
			(index >> (CHUNK_SIZE_LOG2 << 1)) & CHUNK_LOWER_BITS_MASK,
			(index >> CHUNK_SIZE_LOG2) & CHUNK_LOWER_BITS_MASK,
			index & CHUNK_LOWER_BITS_MASK
		};
	};

	// Static data
	struct ManagerInstances
	{
		ChunkRegionManager chunkRegion;
		StructureBlockManager structureBlock;
	};

	static std::unique_ptr<ManagerInstances> managerInstances;
	static CachedBlockIds CACHED_BLOCK_IDS;
	static constexpr std::array<uint32_t, 27> PRECOMPUTED_NEIGHBOR_DIRTY_MASKS = detail::precomputeNeighborDirtyMasks();
	static constexpr bool USE_CONNECTIVITY_TESTING = false;
	static GlobalCounters globalCounters;
private:
	// Data
	glm::ivec3 position;

	std::atomic<State> state{ State::NotInitialized_NeedsBlocks };
	AtomicFlags<uint8_t> chunkFlags;

	uint8_t loaderCount = 0;

	TracyLockableN(AtomicWaitFence, processingFence, "Processing fence");

	Cell cells[CHUNK_VOLUME]; // Storing block and light level close in memory for best cache-locality

	LightPropagationStorage lightPropagation;

	std::array<Chunk*, 27> neighbors{ nullptr }; // Pointers to neighboring chunks, including itself, for easier access
	ChunkRegion* parentRegion = nullptr;

	ChunkIO::BlockChanges blockChanges; // TOOD: Should be thread-safe

	SymmetricBitMatrix<6> sideConnectivity;

	// Data
public:
	ChunkMesh mesh;

	// Constructors, destructors, assigments
	Chunk() = default;
	~Chunk() = default;
	Chunk(const Chunk&)			   = delete;
	Chunk& operator=(const Chunk&) = delete;
	Chunk(Chunk&&)				   = delete;
	Chunk& operator=(Chunk&&)	   = delete;

	// Init/destroy
	void init(const glm::ivec3& newPosition, const std::array<Chunk*, 27>& newNeighbors, ChunkRegion* newParentRegion);
	void destroy();

	static void globalInit();
	static void globalDestroy();

	// Blocks
	void buildBlocks();
	bool hasStructureBlockUpdates() const { return managerInstances->structureBlock.hasPendingChanges(position); };
	void updateStructureBlocks();

	// Light
	void buildLight();
	void updateLight();
	bool shouldUpdateLight() noexcept { return isLightBuilt() && lightPropagation.hasNodes(); }

	// Mesh
	void updateMesh();
	bool shouldUpdateMesh() noexcept { return isLightBuilt() && chunkFlags.readAndSet(Flag::ShouldUpdateMesh, false); };
	void markAsShouldUpdateMesh() noexcept;
	void askForMeshUpload();

	// Connectivity
	void updateConnectivity();
	bool shouldUpdateConnectivity() noexcept { return chunkFlags.readAndSet(Flag::ShouldUpdateConnectivity, false); };
	void markAsShouldUpdateConnectivity() noexcept;

	// Render
	void updateCanBeRenderedFlag() noexcept;
	bool canBeRendered() const noexcept { return readFlag(Flag::CanBeRendered); };

	template<MeshLayer layer>
	void collectRenderData(BufferStreamWriter<DrawArraysIndirectCommand>& drawCommands, BufferStreamWriter<glm::ivec3>& positions) const
	{
		constexpr size_t layerIndex = (size_t)layer;
		//constexpr ChunkMeshFaceStorage::Flag createdFlag = ChunkMeshFaceStorage::createdFlag(layer);

		// Check if face created and it's not empty
		unsigned int faceCount = mesh.faceStorage.renderFaceCounts[layerIndex];
		if (faceCount == 0) // || !mesh.readFlag(createdFlag)
		{
			return;
		}

		// Get offset
		unsigned int offset = mesh.faceStorage.facesBlocks[layerIndex].offset;

		// Push data
		drawCommands.emplaceSingle(4u, faceCount, 0u, offset);
		positions.writeSingle(position);
	}

	// Neighbor dirty mask
	static uint32_t getNeighborDirtyMask(int x, int y, int z) noexcept;
	void applyNeighborDirtyMask(uint32_t mask) noexcept;

	// Grid getters
	BlockId getBlockAt(int x, int y, int z) const noexcept { return cells[getIndex(x, y, z)].block; }
	BlockId getBlockAt(const glm::ivec3& pos) const noexcept { return cells[getIndex(pos)].block; }
	BlockId getBlockAt(size_t index) const noexcept { return cells[index].block; }

	LightLevel getLightAt(int x, int y, int z) const noexcept { return cells[getIndex(x, y, z)].lightLevel; }
	LightLevel getLightAt(const glm::ivec3& pos) const noexcept { return cells[getIndex(pos)].lightLevel; }
	LightLevel getLightAt(size_t index) const noexcept { return cells[index].lightLevel; }

	std::pair<BlockId, LightLevel> getBlockAndLightAt(int x, int y, int z) const noexcept;
	std::pair<BlockId, LightLevel> getBlockAndLightAt(const glm::ivec3& pos) const noexcept;
	std::pair<BlockId, LightLevel> getBlockAndLightAt(size_t index) const noexcept { return std::make_pair(cells[index].block, cells[index].lightLevel); }

	// Grid setters
	void setBlockAtRaw(int x, int y, int z, BlockId block) noexcept { cells[getIndex(x, y, z)].block = block; }
	void setBlockAtRaw(const glm::ivec3& pos, BlockId block) noexcept { cells[getIndex(pos)].block = block; }
	void setBlockAtRaw(size_t index, BlockId block) noexcept { cells[index].block = block; }

	void setLightAtRaw(int x, int y, int z, LightLevel lightLevel) noexcept { cells[getIndex(x, y, z)].lightLevel = lightLevel; }
	void setLightAtRaw(const glm::ivec3& pos, LightLevel lightLevel) noexcept { cells[getIndex(pos)].lightLevel = lightLevel; }
	void setLightAtRaw(size_t index, LightLevel lightLevel) noexcept { cells[index].lightLevel = lightLevel; }

	void setBlockLightAtRaw(int x, int y, int z, uint8_t lightLevel) noexcept { cells[getIndex(x, y, z)].lightLevel.blockLight = lightLevel; }
	void setBlockLightAtRaw(const glm::ivec3& pos, uint8_t lightLevel) noexcept { cells[getIndex(pos)].lightLevel.blockLight = lightLevel; }
	void setBlockLightAtRaw(size_t index, uint8_t lightLevel) noexcept { cells[index].lightLevel.blockLight = lightLevel; }

	void setSkyLightAtRaw(int x, int y, int z, uint8_t lightLevel) noexcept { cells[getIndex(x, y, z)].lightLevel.skyLight = lightLevel; }
	void setSkyLightAtRaw(const glm::ivec3& pos, uint8_t lightLevel) noexcept { cells[getIndex(pos)].lightLevel.skyLight = lightLevel; }
	void setSkyLightAtRaw(size_t index, uint8_t lightLevel) noexcept { cells[index].lightLevel.skyLight = lightLevel; }

	void setBlockAt(int x, int y, int z, BlockId block, bool saveBlockChanges = true);
	void setLightAt(int x, int y, int z, LightLevel lightLevel);
	void setBlockLightAt(int x, int y, int z, uint8_t lightLevel);
	void setSkyLightAt(int x, int y, int z, uint8_t lightLevel);

	void setLightAt(size_t index, LightLevel lightValue);
	void setBlockLightAt(size_t index, uint8_t lightLevel);
	void setSkyLightAt(size_t index, uint8_t lightLevel);

	// Light propagation
	void addBlockLightPropagationNode(int x, int y, int z);
	void addBlockLightRemovalNode(int x, int y, int z, uint8_t lightLevel);
	void addSkyLightPropagationNode(int x, int y, int z);
	void addSkyLightRemovalNode(int x, int y, int z, uint8_t lightLevel);

	// Chunk data getters
	glm::ivec3 getPosition() const noexcept { return position; };
	const auto& getNeighbors() const noexcept { return neighbors; };
	auto getConnectivityMatrix() const noexcept { return sideConnectivity; }
	ChunkRegion* getParentRegion() const noexcept { return parentRegion; }

	// Getters and setters for states and flags
	State getState() const noexcept { return state.load(std::memory_order_acquire); };
	void setState(State newState) noexcept { state.store(newState, std::memory_order_release); }

	void setFlag(Flag flag, bool value) noexcept { chunkFlags.set(static_cast<unsigned>(flag), value); }
	bool readFlag(Flag flag) const noexcept { return chunkFlags.read(static_cast<unsigned>(flag)); }
	bool readAndSetFlag(Flag flag, bool value) noexcept { return chunkFlags.readAndSet(static_cast<unsigned>(flag), value); }

	bool getIsProcessing() const noexcept { return processingFence.m_lockable.isLocked(); };

	bool getIsLoadedInWorld() const noexcept { return chunkFlags.read(Flag::IsLoadedInWorld); };

	bool areBlocksBuilt() const noexcept { return getState() >= State::BlocksBuit_NeedsLight; };
	bool isLightBuilt() const noexcept { return getState() >= State::LightsBuilt; };

	// Getters and setters for loaderCount
	void addLoader() noexcept { loaderCount++; }
	void removeLoader() noexcept { loaderCount--; }
	uint8_t getLoaderCount() const noexcept { return loaderCount; };
private:
	// Blocks
	void generateTree(const glm::ivec3& position);
	void removeBlockChange(BlockId block, uint16_t idx);

	// IO
	void loadSave();
	void save() const;

	// Light
	uint32_t propagateBlockLight();
	uint32_t propagateSkyLight();
	uint32_t propagateBlockLightRemoval();
	uint32_t propagateSkyLightRemoval();

	// Chunk traverse
	Chunk* traverseToSideNeighbor(int x, int y, int z, int side, size_t& outIndex) const;
	Chunk* traverseThroughNeighbors(int x, int y, int z, size_t& outIndex) const;

	// Mesh
	void markMeshesDirtyAroundBlock(int x, int y, int z);

	struct ContextFaceAOAL
	{
		// Out values
		uint32_t outAmbientOcclusion = 0;
		uint32_t outLightLevel = 0;

		// In values
		const glm::ivec3 position;
		const int normal;
		const LightLevel centerFaceLight;
		const bool centerFaceIsSolid = false;
	};

	void calculateVertexAmbientOcclusionAndLight(unsigned int& ao, LightLevel& light, const LightLevel& centerLight, const LightLevelAndIsSolid& side1, const LightLevelAndIsSolid& side2, const LightLevelAndIsSolid& corner) const;
	void calculateFaceAmbientOcclusionAndLight(ContextFaceAOAL& context) const;

	void calculateVertexAmbientOcclusionAndLightUnaligned(unsigned int& ao, LightLevel& light, const LightLevelAndIsSolid& center, const LightLevelAndIsSolid& side1, const LightLevelAndIsSolid& side2, const LightLevelAndIsSolid& corner) const;
	void calculateFaceAmbientOcclusionAndLightUnaligned(ContextFaceAOAL& context) const;
	
	void calculateBlockVertexLight(BlockVertexData& result, const glm::ivec3& currentBlockPosition) const;
};