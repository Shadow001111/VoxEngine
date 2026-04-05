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
}

struct BlockData;

class Chunk
{
	// Types
	struct DirectionsTable
	{
		static constexpr std::array<int, 6> dx{ -1, 1, 0, 0, 0, 0 };
		static constexpr std::array<int, 6> dy{ 0, 0, -1, 1, 0, 0 };
		static constexpr std::array<int, 6> dz{ 0, 0, 0, 0, -1, 1 };
	};

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

	struct BlockVertexLightData
	{
		unsigned int ao[8];      // AO values for each vertex
		LightLevel light[8];     // Light values for each vertex
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
		CanBeRendered
	};
	
	// Static helpers
	static size_t getIndex(int x, int y, int z) noexcept { return (x << (CHUNK_SIZE_LOG2 << 1)) | (y << CHUNK_SIZE_LOG2) | z; };

	static size_t getIndex(int x, int z) noexcept { return (x << CHUNK_SIZE_LOG2) | z; };

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

	static constexpr int getSideNeighborIndex(int side) noexcept
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

	static glm::ivec3 getPositionFromIndex(int index) noexcept // Took 'size_t', but now takes 'int' because think it will be cheaper (less casts)
	{
		return {
			(index >> (CHUNK_SIZE_LOG2 << 1)) & CHUNK_LOWER_BITS_MASK,
			(index >> CHUNK_SIZE_LOG2) & CHUNK_LOWER_BITS_MASK,
			index & CHUNK_LOWER_BITS_MASK
		};
	};

	// Static data
	static std::atomic<bool> gHasStructureBlockChanges; // TODO: Move this to StructureBlockChangeManager.
	static std::unique_ptr<ChunkRegionManager> chunkRegionManagerInstance;
	static CachedBlockIds CACHED_BLOCK_IDS;
	static constexpr std::array<uint32_t, 27> PRECOMPUTED_NEIGHBOR_DIRTY_MASKS = detail::precomputeNeighborDirtyMasks();

	// Static data
private:
	static thread_local ChunkInstancedMeshFaceStorage::InstancesStorage localMeshInstances;
	static StructureBlockChangeManager structureBlockChangeManager;

	// Data
	glm::ivec3 position;

	std::atomic<State> state = State::NotInitialized_NeedsBlocks;
	AtomicFlags<uint8_t> chunkFlags;

	uint8_t loaderCount = 0;

	AtomicWaitFence processingFence;

	BlockId blocks[CHUNK_VOLUME];
	LightLevel lightLevels[CHUNK_VOLUME];

	LightPropagationStorage lightPropagation;

	std::array<Chunk*, 27> neighbors{ nullptr }; // Pointers to neighboring chunks, including itself, for easier access
	ChunkRegion* parentRegion = nullptr;

	ChunkIO::BlockChanges changedBlocks;

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
	void init(const glm::ivec3& position, const std::array<Chunk*, 27>& newNeighbors, ChunkRegion* parentRegion);
	void destroy();

	static void globalInit();
	static void globalDestroy();

	// Blocks
	void buildBlocks();
	bool hasStructureBlockUpdates() const { return structureBlockChangeManager.hasPendingChanges(position); };
	void updateStructureBlocks();

	// Light
	void buildLight();
	void updateLight();
	bool shouldUpdateLight() noexcept { return isLightBuilt() && lightPropagation.hasNodes(); }

	// Mesh
	void updateMesh();
	bool shouldUpdateMesh() noexcept { return isLightBuilt() && chunkFlags.readAndSet(Flag::ShouldUpdateMesh, false); };
	void markMeshDirty();
	void askForMeshUpload();

	// Render
	void updateCanBeRenderedFlag() noexcept;
	bool canBeRendered() const noexcept { return readFlag(Flag::CanBeRendered); };

	void collectAlignedOpaqueRenderData(BufferStreamWriter<DrawArraysIndirectCommand>& drawCommands, BufferStreamWriter<glm::ivec3>& positions) const;
	void collectAlignedTranslucentRenderData(BufferStreamWriter<DrawArraysIndirectCommand>& drawCommands, BufferStreamWriter<glm::ivec3>& positions) const;
	void collectNonAlignedOpaqueRenderData(BufferStreamWriter<DrawArraysIndirectCommand>& drawCommands, BufferStreamWriter<glm::ivec3>& positions) const;
	void collectNonAlignedTranslucentRenderData(BufferStreamWriter<DrawArraysIndirectCommand>& drawCommands, BufferStreamWriter<glm::ivec3>& positions) const;

	// Neighbor dirty mask
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
	void addBlockLightPropagationNode(int x, int y, int z);
	void addBlockLightRemovalNode(int x, int y, int z, uint8_t lightLevel);
	void addSkyLightPropagationNode(int x, int y, int z);
	void addSkyLightRemovalNode(int x, int y, int z, uint8_t lightLevel);

	// Chunk data getters
	glm::ivec3 getPosition() const noexcept { return position; };
	const auto& getNeighbors() const noexcept { return neighbors; };
	size_t getFaceCount() const noexcept { return mesh.faceStorage.getAllFaceCount(); };

	// Getters and setters for states and flags
	State getState() const noexcept { return state.load(std::memory_order_acquire); };
	void setState(State newState) noexcept { state.store(newState, std::memory_order_release); }

	void setFlag(Flag flag, bool value) noexcept { chunkFlags.set(static_cast<unsigned>(flag), value); }
	bool readFlag(Flag flag) const noexcept { return chunkFlags.read(static_cast<unsigned>(flag)); }
	bool readAndSetFlag(Flag flag, bool value) noexcept { return chunkFlags.readAndSet(static_cast<unsigned>(flag), value); }

	bool getIsProcessing() const noexcept { return processingFence.isProcessing(); };

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
		const int x, y, z;
		const int normal;
		const LightLevel centerFaceLight;
	};

	void calculateVertexAmbientOcclusionAndLight(unsigned int& ao, LightLevel& light, const LightLevel& centerLight, const LightLevelAndIsSolid& side1, const LightLevelAndIsSolid& side2, const LightLevelAndIsSolid& corner) const;
	void calculateFaceAmbientOcclusionAndLight(ContextFaceAOAL& context) const;
	
	void calculateBlockVertexLight(BlockVertexLightData& result, int x, int y, int z) const;
};