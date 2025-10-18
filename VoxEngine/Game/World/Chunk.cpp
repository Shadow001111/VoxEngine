#include "Chunk.h"
#include "Chunk/TerrainGenerator.h"

#include "Core/Profiler.h"

#include <cassert>
#include <vector>
#include <iostream>

BlockTextureIDDatabase Chunk::blockTextureDatabase;

inline size_t Chunk::getIndex(int x, int y, int z)
{
	return (x << 8) | (y << 4) | z;
}

Chunk::Chunk()
{
}

Chunk::~Chunk()
{
	destroy(); // Just in case
}

inline bool Chunk::operator==(const Chunk& other) const
{
	return position == other.position;
}

// Prepares chunk for use
void Chunk::init(int x, int y, int z, Chunk** neighbors)
{
	// Set position
	position = { x, y, z };

	// Set neighbours
	for (int i = 0; i < 6; i++)
	{
		Chunk* neighbor = neighbors[i];
		this->neighbors[i] = neighbor;
		if (neighbor)
		{
			neighbor->neighbors[i ^ 1] = this;
		}
	}

	// Reset
	setState(Chunk::State::NeedsBlocks);
	isLoadedInWorld.store(true, std::memory_order_release);
	isLoadedChunkColumnData.store(false, std::memory_order_release);
	areBlocksBuilt.store(false, std::memory_order_release);
	isLightBuilt.store(false, std::memory_order_release);

	assert(!meshData.processingFence.isProcessing());
	meshData.ready = false;
	meshData.opaqueDirty = false;
	meshData.transparentDirty = false;
}

// Cleans up resources
void Chunk::destroy()
{
	// Clear neighbors
	for (int i = 0; i < 6; i++)
	{
		Chunk* neighbor = neighbors[i];
		if (neighbor)
		{
			neighbor->neighbors[i ^ 1] = nullptr;
			neighbors[i] = nullptr;
		}
	}

	//
	isLoadedInWorld.store(false, std::memory_order_release);
	meshData.opaqueInstances.clear();

	// Release chunk column data
	if (isLoadedChunkColumnData.load(std::memory_order_acquire))
	{
		isLoadedChunkColumnData.store(false, std::memory_order_release);
		TerrainGenerator::getInstance().unloadChunkColumnData(position.x, position.z);
	}

	//
	{
		std::lock_guard<std::mutex> lock(lightMutex);
		while (!lightQueue.empty())
		{
			lightQueue.pop();
		}
	}
}

// Fills 'blocks' array
void Chunk::buildBlocks()
{
	if (
		areBlocksBuilt.load(std::memory_order_acquire) ||
		!isLoadedInWorld.load(std::memory_order_acquire)
	)
	{
		return;
	}

	Profiler::beginProfile("Build chunk blocks: wait", ProfileCategory::ChunkBlocks);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	// Load chunk column data
	const ChunkColumnData* chunkColumnData = TerrainGenerator::getInstance().loadChunkColumnData(position.x, position.z);
	const int* heightMap = chunkColumnData->heightMapRead();
	isLoadedChunkColumnData.store(true, std::memory_order_release);

	// Terrain
	bool computeCaveMask = false;
	{
		PROFILE_SCOPE("Build chunk blocks", ProfileCategory::ChunkBlocks);

		const int globalChunkY = position.y * CHUNK_SIZE;
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				int globalHeight = heightMap[z + x * CHUNK_SIZE];
				int localheight = std::min(CHUNK_SIZE - 1, globalHeight - globalChunkY);
				for (int y = 0; y < CHUNK_SIZE; y++)
				{
					int globalY = globalChunkY + y;

					size_t index = getIndex(x, y, z);

					Block block = Block::Air;
					if (globalY > globalHeight)
					{
						block = Block::Air;
					}
					else if (globalY == globalHeight)
					{
						block = Block::GrassBlock;
					}
					else if (globalY > globalHeight - 4)
					{
						block = Block::Dirt;
					}
					else
					{
						block = Block::Stone;
						computeCaveMask = true;
					}

					blocks[index] = block;
				}
			}
		}

		if (position.y <= 1)
		{
			int border = 7;
			for (int x = border; x < CHUNK_SIZE - border; x++)
			{
				for (int y = border; y < CHUNK_SIZE - border; y++)
				{
					for (int z = border; z < CHUNK_SIZE - border; z++)
					{
						if ((x + y + z) & 1)
						{
							blocks[getIndex(x, y, z)] = Block::ColoredGlass;
						}
					}
				}
			}
		}
	}

	// Caves
	if (computeCaveMask)
	{
		bool caveMask[CHUNK_VOLUME];
		TerrainGenerator::getInstance().computeCaveMask(caveMask, position.x, position.y, position.z);

		PROFILE_SCOPE("Build chunk blocks: caves", ProfileCategory::ChunkBlocks);

		for (int i = 0; i < CHUNK_VOLUME; i++)
		{
			if (blocks[i] == Block::Stone && caveMask[i])
			{
				blocks[i] = Block::Air;
			}
		}
	}

	areBlocksBuilt.store(true, std::memory_order_release);
}

void Chunk::buildLight()
{
	if (
		!isLoadedInWorld.load(std::memory_order_acquire) ||
		!areBlocksBuilt.load(std::memory_order_acquire)
		)
	{
		return;
	}

	Profiler::beginProfile("Build chunk light: wait", ProfileCategory::ChunkLight);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	//
	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };

	// Initialize all light values to 0
	std::fill(std::begin(light), std::end(light), 0);

	// Step 1: Collect light sources
	std::queue<LightNode> localLightQueue;
	{
		PROFILE_SCOPE("Build chunk light: collect light sources", ProfileCategory::ChunkLight);

		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					size_t index = getIndex(x, y, z);
					light[index] = 15 << 4;

					Block currentBlock = blocks[index];
					const BlockData* currentBlockData = BlockDataBase::getBlockData(currentBlock);

					uint8_t emission = currentBlockData->properties.lightEmission;
					if (emission == 0)
					{
						continue;
					}

					if (currentBlockData->properties.areFacesTransparent)
					{
						localLightQueue.emplace(x, y, z, emission, -1);
					}

					for (int i = 0; i < 6; i++)
					{
						int nx = x + dx[i];
						int ny = y + dy[i];
						int nz = z + dz[i];

						if ((nx & CHUNK_UPPER_BITS_MASK) == 0 &&
							(ny & CHUNK_UPPER_BITS_MASK) == 0 &&
							(nz & CHUNK_UPPER_BITS_MASK) == 0)
						{
							localLightQueue.emplace(nx, ny, nz, emission, i);
						}
						else
						{
							Chunk* neighbor = neighbors[i];
							if (neighbor == nullptr)
							{
								continue;
							}

							int neighborLocalX = nx & CHUNK_LOWER_BITS_MASK;
							int neighborLocalY = ny & CHUNK_LOWER_BITS_MASK;
							int neighborLocalZ = nz & CHUNK_LOWER_BITS_MASK;

							neighbor->addNodeToLightQueue(neighborLocalX, neighborLocalY, neighborLocalZ, emission, i);
						}
					}
				}
			}
		}
	}

	// Step 2: Propagate light using flood-fill
	{
		PROFILE_SCOPE("Build chunk light: flood-fill", ProfileCategory::ChunkLight);

		while (!localLightQueue.empty())
		{
			auto data = localLightQueue.front();
			localLightQueue.pop();

			// Get node data
			int x = data.x;
			int y = data.y;
			int z = data.z;
			uint8_t blockLight = data.lightLevel & 15;
			int8_t propagationSide = data.propagationSide;

			// Get block data
			size_t index = getIndex(x, y, z);
			Block block = blocks[index];
			const BlockData* blockData = BlockDataBase::getBlockData(block);

			uint8_t lightAbsorption = blockData->properties.lightAbsorption;
			assert(lightAbsorption > 0);

			// Calculate new light
			uint8_t newBlockLight;
			if (propagationSide == -1)
			{
				newBlockLight = blockLight;
			}
			else
			{
				newBlockLight = blockLight > lightAbsorption ? blockLight - lightAbsorption : 0;
			}

			if (newBlockLight == 0)
			{
				continue;
			}

			// Get current light
			uint8_t currentLight = light[index];
			uint8_t currentBlockLight = currentLight & 15;
			uint8_t currentSkyLight = currentLight << 4;

			// Compare light values
			if (newBlockLight <= currentBlockLight)
			{
				continue;
			}

			// Store new value
			light[index] = currentSkyLight | newBlockLight;

			// Early exit, because spreading light value of 1 will do nothing
			if (newBlockLight == 1)
			{
				continue;
			}

			// Propagate to 6 neighbors
			for (int i = 0; i < 6; i++)
			{
				if (i == (propagationSide ^ 1))
				{
					continue;
				}

				int nx = x + dx[i];
				int ny = y + dy[i];
				int nz = z + dz[i];

				if ((nx & CHUNK_UPPER_BITS_MASK) == 0 &&
					(ny & CHUNK_UPPER_BITS_MASK) == 0 &&
					(nz & CHUNK_UPPER_BITS_MASK) == 0)
				{
					localLightQueue.emplace(nx, ny, nz, newBlockLight, i);
				}
				else
				{
					Chunk* neighbor = neighbors[i];
					if (neighbor == nullptr)
					{
						continue;
					}

					int neighborLocalX = nx & CHUNK_LOWER_BITS_MASK;
					int neighborLocalY = ny & CHUNK_LOWER_BITS_MASK;
					int neighborLocalZ = nz & CHUNK_LOWER_BITS_MASK;

					neighbor->addNodeToLightQueue(neighborLocalX, neighborLocalY, neighborLocalZ, newBlockLight, i);
				}
			}
		}
	}

	isLightBuilt.store(true, std::memory_order_release);
}

void Chunk::buildMesh()
{
	if (
		!isLoadedInWorld.load(std::memory_order_acquire) ||
		!areBlocksBuilt.load(std::memory_order_acquire) ||
		!isLightBuilt.load(std::memory_order_acquire)
		)
	{
		return;
	}

	// TODO: Mesh should be sorted after first build.

	Profiler::beginProfile("Build chunk mesh: wait", ProfileCategory::ChunkMesh);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	meshData.ready = false;
	meshData.opaqueDirty = false;
	meshData.transparentDirty = false;

	{
		PROFILE_SCOPE("Build chunk mesh", ProfileCategory::ChunkMesh);

		// Collect visible faces
		std::vector<BlockFaceInstance> instances[12];
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			for (int y = 0; y < CHUNK_SIZE; y++)
			{
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					Block block = getBlock_inBoundaries(x, y, z);
					const BlockData* blockData = BlockDataBase::getBlockData(block);
					if (!blockData->properties.hasFaces)
					{
						continue;
					}

					const auto& textureIDs = blockTextureDatabase.getBlockTextureIDs(block);
					int instanceArrayOffset = blockData->properties.areFacesTransparent ? 6 : 0;

					// I tried to do a loop, but it doubles the execution time

					// -X
					std::pair<Block, uint8_t> blockAndLight = getBlockAndLight_checkSideNeighbor(x - 1, y, z, 0);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 0, blockAndLight.second);
						instances[instanceArrayOffset].emplace_back(x, y, z, 0, ao, textureIDs.ids[0], light);
					}

					// +X
					blockAndLight = getBlockAndLight_checkSideNeighbor(x + 1, y, z, 1);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 1, blockAndLight.second);
						instances[1 + instanceArrayOffset].emplace_back(x, y, z, 1, ao, textureIDs.ids[1], light);
					}

					// -Y
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y - 1, z, 2);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 2, blockAndLight.second);
						instances[2 + instanceArrayOffset].emplace_back(x, y, z, 2, ao, textureIDs.ids[2], light);
					}

					// +Y
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y + 1, z, 3);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 3, blockAndLight.second);
						instances[3 + instanceArrayOffset].emplace_back(x, y, z, 3, ao, textureIDs.ids[3], light);
					}

					// -Z
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y, z - 1, 4);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 4, blockAndLight.second);
						instances[4 + instanceArrayOffset].emplace_back(x, y, z, 4, ao, textureIDs.ids[4], light);
					}

					// +Z
					blockAndLight = getBlockAndLight_checkSideNeighbor(x, y, z + 1, 5);
					blockData = BlockDataBase::getBlockData(blockAndLight.first);
					if (blockData->properties.areFacesTransparent && block != blockAndLight.first)
					{
						unsigned int ao, light;
						calculateFaceAmbientOcclusionAndLight(ao, light, x, y, z, 5, blockAndLight.second);
						instances[5 + instanceArrayOffset].emplace_back(x, y, z, 5, ao, textureIDs.ids[5], light);
					}
				}
			}
		}

		// Combine instances vectors
		for (int i = 0; i < 6; i++)
		{
			meshData.opaqueFaceCount[i] = instances[i].size();
			meshData.transparentFaceCount[i] = instances[i + 6].size();
		}

		assert(!meshData.processingFence.isProcessing());

		meshData.opaqueInstances.clear();
		meshData.opaqueInstances.reserve(meshData.getOpaqueFaceCountSum());
		for (int i = 0; i < 6; i++)
		{
			const auto& vectorToInsert = instances[i];
			meshData.opaqueInstances.insert(meshData.opaqueInstances.end(), vectorToInsert.begin(), vectorToInsert.end());
		}

		meshData.transparentInstances.clear();
		meshData.transparentInstances.reserve(meshData.getTransparentFaceCountSum());
		for (int i = 0; i < 6; i++)
		{
			const auto& vectorToInsert = instances[6 + i];
			meshData.transparentInstances.insert(meshData.transparentInstances.end(), vectorToInsert.begin(), vectorToInsert.end());
		}
	}

	if (!isLoadedInWorld.load(std::memory_order_acquire))
	{
		return;
	}

	if (meshData.getOpaqueFaceCountSum() > 0)
	{
		meshData.opaqueDirty = true;
	}
	if (meshData.getTransparentFaceCountSum() > 0)
	{
		meshData.transparentDirty = true;
	}
}

void Chunk::updateLight()
{
	if (
		!isLoadedInWorld.load(std::memory_order_acquire) ||
		!areBlocksBuilt.load(std::memory_order_acquire)
		)
	{
		return;
	}

	Profiler::beginProfile("Update chunk light: wait", ProfileCategory::ChunkLight);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	PROFILE_SCOPE("Update chunk light", ProfileCategory::ChunkLight);

	// Get pending light updates
	std::queue<LightNode> localLightQueue;
	{
		std::lock_guard<std::mutex> lock(lightMutex);
		if (lightQueue.empty())
		{
			return;
		}
		localLightQueue.swap(lightQueue);
	}

	const int dx[] = { -1, 1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, -1, 1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, -1, 1 };

	bool lightChanged = false;

	// Process light propagation
	while (!localLightQueue.empty())
	{
		auto data = localLightQueue.front();
		localLightQueue.pop();

		// Get node data
		int x = data.x;
		int y = data.y;
		int z = data.z;
		uint8_t blockLight = data.lightLevel & 15;
		int8_t propagationSide = data.propagationSide;

		// Get block data
		size_t index = getIndex(x, y, z);
		Block block = blocks[index];
		const BlockData* blockData = BlockDataBase::getBlockData(block);

		uint8_t lightAbsorption = blockData->properties.lightAbsorption;
		assert(lightAbsorption > 0);

		// Calculate new light
		uint8_t newBlockLight;
		if (propagationSide == -1)
		{
			newBlockLight = blockLight;
		}
		else
		{
			newBlockLight = blockLight > lightAbsorption ? blockLight - lightAbsorption : 0;
		}

		if (newBlockLight == 0)
		{
			continue;
		}

		// Get current light
		uint8_t currentLight = light[index];
		uint8_t currentBlockLight = currentLight & 15;
		uint8_t currentSkyLight = currentLight << 4;

		// Compare light values
		if (newBlockLight <= currentBlockLight)
		{
			continue;
		}

		// Store new value
		light[index] = currentSkyLight | newBlockLight;

		// Early exit, because spreading light value of 1 will do nothing
		if (newBlockLight == 1)
		{
			continue;
		}

		// Propagate to 6 neighbors
		for (int i = 0; i < 6; i++)
		{
			if (i == (propagationSide ^ 1))
			{
				continue;
			}

			int nx = x + dx[i];
			int ny = y + dy[i];
			int nz = z + dz[i];

			if ((nx & CHUNK_UPPER_BITS_MASK) == 0 &&
				(ny & CHUNK_UPPER_BITS_MASK) == 0 &&
				(nz & CHUNK_UPPER_BITS_MASK) == 0)
			{
				localLightQueue.emplace(nx, ny, nz, newBlockLight, i);
			}
			else
			{
				Chunk* neighbor = neighbors[i];
				if (neighbor == nullptr)
				{
					continue;
				}

				int neighborLocalX = nx & CHUNK_LOWER_BITS_MASK;
				int neighborLocalY = ny & CHUNK_LOWER_BITS_MASK;
				int neighborLocalZ = nz & CHUNK_LOWER_BITS_MASK;

				neighbor->addNodeToLightQueue(neighborLocalX, neighborLocalY, neighborLocalZ, newBlockLight, i);
			}
		}
	}

	// If light changed, mark for mesh rebuild
	if (lightChanged && getState() == State::Ready)
	{
		setState(State::NeedsMesh);
	}
}

bool Chunk::hasLightUpdates() const
{
	std::lock_guard<std::mutex> lock(lightMutex);
	return !lightQueue.empty();
}

void Chunk::sortMesh(const glm::ivec3& cameraBlockPos)
{
	if (meshData.transparentInstances.empty())
	{
		return;
	}

	const glm::ivec3 chunkGlobalPos = position * CHUNK_SIZE;
	const glm::ivec3 chunkGlobalPosBlockMax = chunkGlobalPos + (CHUNK_SIZE - 1);

	glm::ivec3 newCameraClosestBlockPosForSortingMesh = glm::clamp(cameraBlockPos, chunkGlobalPos, chunkGlobalPosBlockMax);
	if (newCameraClosestBlockPosForSortingMesh == cameraClosestBlockPosForSortingMesh)
	{
		return;
	}
	cameraClosestBlockPosForSortingMesh = newCameraClosestBlockPosForSortingMesh;

	const glm::ivec3 chunkGlobalPosMinusCameraPos = chunkGlobalPos - cameraBlockPos;

	PROFILE_SCOPE("Sort chunk mesh", ProfileCategory::ChunkMesh);

	// Sorting only transparent faces for now
	// If GL_CULL_FACE is disabled, sorting must be adjusted. If faces have same position, they should be sorted by prioritized normal(something opposite to camera direction).
	// TODO: Consider sorting opaque faces to reduce overdraw
	// TODO: Consider using bucket sort, because there alot repeating distances

	// Shouldn't be set, because it's barely noticeable when order of faces changes.
	// meshData.ready = false;

	struct FaceSortStruct
	{
		const BlockFaceInstance* instance = nullptr;
		unsigned int manhattanDistance = 0;

		FaceSortStruct(const BlockFaceInstance* instance, unsigned int manhattanDistance) :
			instance(instance), manhattanDistance(manhattanDistance)
		{}

		FaceSortStruct& operator=(const FaceSortStruct& other)
		{
			if (this != &other)
			{
				instance = other.instance;
				manhattanDistance = other.manhattanDistance;
			}
			return *this;
		}
	};

	// Collect
	std::vector<FaceSortStruct> faceSortVector;
	faceSortVector.reserve(meshData.transparentInstances.size());
	for (const auto& instance : meshData.transparentInstances)
	{
		glm::ivec3 pos;
		instance.decodePosition(pos.x, pos.y, pos.z);

		glm::ivec3 delta = glm::abs(pos + chunkGlobalPosMinusCameraPos);

		unsigned int manhattanDistance = delta.x + delta.y + delta.z;

		faceSortVector.emplace_back(&instance, manhattanDistance);
	}

	// Sorting faces in descending order, so furthest transparent faces will be rendered first, for blending
	std::sort(faceSortVector.begin(), faceSortVector.end(),
		[](const FaceSortStruct& a, const FaceSortStruct& b)
		{
			return a.manhattanDistance > b.manhattanDistance;
		});

	// Reordering instances
	std::vector<BlockFaceInstance> reordered;
	reordered.reserve(meshData.transparentInstances.capacity());

	for (const auto& face : faceSortVector)
	{
		reordered.push_back(*face.instance);
	}

	meshData.transparentInstances = std::move(reordered);
	
	// Done
	meshData.transparentDirty = true;
}

void Chunk::sendMeshToGPU()
{
	// Commented out some conditions, because this method runs on main thread
	if (
		//!isLoadedInWorld.load(std::memory_order_acquire) ||
		!areBlocksBuilt.load(std::memory_order_acquire) ||
		!isLightBuilt.load(std::memory_order_acquire) ||
		!(meshData.opaqueDirty || meshData.transparentDirty)
		)
	{
		return;
	}

	Profiler::beginProfile("Send chunk mesh to GPU: wait", ProfileCategory::ChunkMesh);
	ScopedProcessingFence scopedFence(processingFence);
	Profiler::endProfile();

	{
		PROFILE_SCOPE("Send chunk mesh to GPU", ProfileCategory::ChunkMesh);

		// Bind instance VBO
		meshData.bindInstanceVBO();

		// Allocating data for buffer
		size_t opaqueFaceCount = meshData.getOpaqueFaceCountSum();
		size_t transparentFaceCount = meshData.getTransparentFaceCountSum();
		meshData.allocateMemoryForBuffer(opaqueFaceCount + transparentFaceCount);

		// Write to buffer
		if (meshData.opaqueDirty)
		{
			glBufferSubData(GL_ARRAY_BUFFER,
				0,
				opaqueFaceCount * sizeof(BlockFaceInstance),
				meshData.opaqueInstances.data()
			);
		}

		if (meshData.transparentDirty)
		{
			glBufferSubData(GL_ARRAY_BUFFER,
				opaqueFaceCount * sizeof(BlockFaceInstance),
				transparentFaceCount * sizeof(BlockFaceInstance),
				meshData.transparentInstances.data()
			);
		}

		// Done
		meshData.ready = true;
		meshData.opaqueDirty = false;
		meshData.transparentDirty = false;

		// Don't clear instaces data, because it's needed for mesh sorting.
		//meshData->opaqueInstances.clear();
		//meshData->transparentInstances.clear();
	}
}

void Chunk::render(bool transparent) const
{
	if (canBeRendered(transparent))
	{
		size_t opaqueFaceCount = meshData.getOpaqueFaceCountSum();
		size_t baseInstance = transparent ? opaqueFaceCount : 0;
		size_t instanceCount = transparent ? meshData.getTransparentFaceCountSum() : opaqueFaceCount;
		meshData.bindVAO();
		glDrawArraysInstancedBaseInstance(GL_TRIANGLE_FAN, 0, 4, instanceCount, baseInstance);
	}
}

bool Chunk::canBeRendered(bool transparent) const
{
	size_t opaqueFaceCount = meshData.getOpaqueFaceCountSum();
	size_t faceCapacity = meshData.getFaceCapacity();

	size_t faceCount = transparent ? meshData.getTransparentFaceCountSum() : opaqueFaceCount;
	size_t faceOffset = transparent ? opaqueFaceCount : 0;
	return
		getState() == State::Ready &&
		meshData.ready &&
		faceCount > 0 &&
		(faceCount + faceOffset) <= faceCapacity &&
		!processingFence.isProcessing();// &&
		//!meshData.processingFence.isProcessing();
}

bool Chunk::canBeRendered() const
{
	size_t opaqueFaceCount = meshData.getOpaqueFaceCountSum();
	size_t transparentFaceCount = meshData.getTransparentFaceCountSum();
	size_t faceCapacity = meshData.getFaceCapacity();
	return
		getState() == State::Ready &&
		meshData.ready &&
		(opaqueFaceCount > 0 || transparentFaceCount > 0) &&
		(opaqueFaceCount + transparentFaceCount) <= faceCapacity &&
		!processingFence.isProcessing();// &&
	//!meshData.processingFence.isProcessing();
}

// Function doesn't check for boundaries, it trusts the caller. On debug mode, it asserts.
Block Chunk::getBlock_inBoundaries(int x, int y, int z) const
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	return blocks[getIndex(x, y, z)];
}

Block Chunk::getBlock_checkSideNeighbor(int x, int y, int z, int side) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx == 0 && ny == 0 && nz == 0)
	{
		return blocks[getIndex(x, y, z)];
	}

	const Chunk* neighbor = neighbors[side];
	if (neighbor)
	{
		return neighbor->getBlock_inBoundaries(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
	}
	return Block::Air;
}

// Function checks neighbors, if out of boundaries. Handles diagonal neighbors too.
Block Chunk::getBlock_checkNeighborsTraverse(int x, int y, int z) const
{
	// Note: Traverse may fail depending on traversal order. Isn't critical for ambient occlusion computing.

	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	// If within current chunk bounds
	if (nx == 0 && ny == 0 && nz == 0)
	{
		return blocks[getIndex(x, y, z)];
	}

	// Determine which direction(s) we need to traverse
	int dirX = (nx < 0) ? 0 : ((nx > 0) ? 1 : -1); // -1 means no X traversal
	int dirY = (ny < 0) ? 2 : ((ny > 0) ? 3 : -1); // -1 means no Y traversal
	int dirZ = (nz < 0) ? 4 : ((nz > 0) ? 5 : -1); // -1 means no Z traversal

	const Chunk* neighbor = this;

	// Traverse in X direction first
	if (dirX != -1)
	{
		neighbor = neighbor->neighbors[dirX];
		if (!neighbor) return Block::Air;
	}

	// Then traverse in Y direction
	if (dirY != -1)
	{
		neighbor = neighbor->neighbors[dirY];
		if (!neighbor) return Block::Air;
	}

	// Finally traverse in Z direction
	if (dirZ != -1)
	{
		neighbor = neighbor->neighbors[dirZ];
		if (!neighbor) return Block::Air;
	}

	// Get local coordinates in the final neighbor chunk
	int localX = x & CHUNK_LOWER_BITS_MASK;
	int localY = y & CHUNK_LOWER_BITS_MASK;
	int localZ = z & CHUNK_LOWER_BITS_MASK;

	return neighbor->getBlock_inBoundaries(localX, localY, localZ);
}

void Chunk::setBlock_inBoundaries(int x, int y, int z, Block block)
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	blocks[getIndex(x, y, z)] = block;
}

uint8_t Chunk::getLight_inBoundaries(int x, int y, int z) const
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	return light[getIndex(x, y, z)];
}

uint8_t Chunk::getLight_checkSideNeighbor(int x, int y, int z, int side) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx == 0 && ny == 0 && nz == 0)
	{
		return light[getIndex(x, y, z)];
	}

	const Chunk* neighbor = neighbors[side];
	if (neighbor)
	{
		return neighbor->getLight_inBoundaries(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
	}
	return 15 << 4;
}

uint8_t Chunk::getLight_checkNeighborsTraverse(int x, int y, int z) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	// If within current chunk bounds
	if (nx == 0 && ny == 0 && nz == 0)
	{
		return light[getIndex(x, y, z)];
	}

	// Determine which direction(s) we need to traverse
	int dirX = (nx < 0) ? 0 : ((nx > 0) ? 1 : -1); // -1 means no X traversal
	int dirY = (ny < 0) ? 2 : ((ny > 0) ? 3 : -1); // -1 means no Y traversal
	int dirZ = (nz < 0) ? 4 : ((nz > 0) ? 5 : -1); // -1 means no Z traversal

	const Chunk* neighbor = this;

	// Traverse in X direction first
	if (dirX != -1)
	{
		neighbor = neighbor->neighbors[dirX];
		if (!neighbor) return 15 << 4;
	}

	// Then traverse in Y direction
	if (dirY != -1)
	{
		neighbor = neighbor->neighbors[dirY];
		if (!neighbor) return 15 << 4;
	}

	// Finally traverse in Z direction
	if (dirZ != -1)
	{
		neighbor = neighbor->neighbors[dirZ];
		if (!neighbor) return 15 << 4;
	}

	// Get local coordinates in the final neighbor chunk
	int localX = x & CHUNK_LOWER_BITS_MASK;
	int localY = y & CHUNK_LOWER_BITS_MASK;
	int localZ = z & CHUNK_LOWER_BITS_MASK;

	return neighbor->getLight_inBoundaries(localX, localY, localZ);
}

void Chunk::setLight_inBoundaries(int x, int y, int z, uint8_t lightValue)
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	light[getIndex(x, y, z)] = lightValue;
}

std::pair<Block, uint8_t> Chunk::getBlockAndLight_inBoundaries(int x, int y, int z) const
{
	assert(x >= 0 && x < CHUNK_SIZE);
	assert(y >= 0 && y < CHUNK_SIZE);
	assert(z >= 0 && z < CHUNK_SIZE);
	return std::make_pair(blocks[getIndex(x, y, z)], light[getIndex(x, y, z)]);
}

std::pair<Block, uint8_t> Chunk::getBlockAndLight_checkSideNeighbor(int x, int y, int z, int side) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	if (nx == 0 && ny == 0 && nz == 0)
	{
		return std::make_pair(blocks[getIndex(x, y, z)], light[getIndex(x, y, z)]);
	}

	const Chunk* neighbor = neighbors[side];
	if (neighbor)
	{
		return neighbor->getBlockAndLight_inBoundaries(x & CHUNK_LOWER_BITS_MASK, y & CHUNK_LOWER_BITS_MASK, z & CHUNK_LOWER_BITS_MASK);
	}
	return std::make_pair(Block::Air, 15 << 4);
}

std::pair<Block, uint8_t> Chunk::getBlockAndLight_checkNeighborsTraverse(int x, int y, int z) const
{
	int nx = x & CHUNK_UPPER_BITS_MASK;
	int ny = y & CHUNK_UPPER_BITS_MASK;
	int nz = z & CHUNK_UPPER_BITS_MASK;

	// If within current chunk bounds
	if (nx == 0 && ny == 0 && nz == 0)
	{
		return std::make_pair(blocks[getIndex(x, y, z)], light[getIndex(x, y, z)]);
	}

	// Determine which direction(s) we need to traverse
	int dirX = (nx < 0) ? 0 : ((nx > 0) ? 1 : -1); // -1 means no X traversal
	int dirY = (ny < 0) ? 2 : ((ny > 0) ? 3 : -1); // -1 means no Y traversal
	int dirZ = (nz < 0) ? 4 : ((nz > 0) ? 5 : -1); // -1 means no Z traversal

	const Chunk* neighbor = this;

	// Traverse in X direction first
	if (dirX != -1)
	{
		neighbor = neighbor->neighbors[dirX];
		if (!neighbor) return std::make_pair(Block::Air, 15 << 4);
	}

	// Then traverse in Y direction
	if (dirY != -1)
	{
		neighbor = neighbor->neighbors[dirY];
		if (!neighbor) return std::make_pair(Block::Air, 15 << 4);
	}

	// Finally traverse in Z direction
	if (dirZ != -1)
	{
		neighbor = neighbor->neighbors[dirZ];
		if (!neighbor) return std::make_pair(Block::Air, 15 << 4);
	}

	// Get local coordinates in the final neighbor chunk
	int localX = x & CHUNK_LOWER_BITS_MASK;
	int localY = y & CHUNK_LOWER_BITS_MASK;
	int localZ = z & CHUNK_LOWER_BITS_MASK;

	return neighbor->getBlockAndLight_inBoundaries(localX, localY, localZ);
}

void Chunk::addNodeToLightQueue(int x, int y, int z, uint8_t lightLevel, int8_t propagationSide)
{
	std::lock_guard<std::mutex> lock(lightMutex);
	lightQueue.emplace(x, y, z, lightLevel, propagationSide);
}

void Chunk::calculateVertexAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, uint8_t centerLight, uint8_t side1Light, uint8_t side2Light, uint8_t cornerLight, bool side1Solid, bool side2Solid, bool cornerSolid) const
{
	unsigned int blockLightSum = centerLight & 15;
	unsigned int skyLightSum = (centerLight >> 4) & 15;
	unsigned int count = 1;

	if (side1Solid && side2Solid)
	{
		ao = 0;
		light = (blockLightSum & 15) | ((skyLightSum & 15) << 4);
		return;
	}

	if (!side1Solid)
	{
		blockLightSum += side1Light & 15;
		skyLightSum += (side1Light >> 4) & 15;
		count++;
	}

	if (!side2Solid)
	{
		blockLightSum += side2Light & 15;
		skyLightSum += (side2Light >> 4) & 15;
		count++;
	}

	if (!side1Solid && !side2Solid && !cornerSolid)
	{
		blockLightSum += cornerLight & 15;
		skyLightSum += (cornerLight >> 4) & 15;
		count++;
	}

	unsigned int avgBlockLight = blockLightSum / count;
	unsigned int avgSkyLight = skyLightSum / count;

	assert(avgBlockLight >= 0 && avgBlockLight <= 15);
	assert(avgSkyLight >= 0 && avgSkyLight <= 15);

	ao = 3 - (side1Solid + side2Solid + cornerSolid);
	light = (avgBlockLight & 15) | ((avgSkyLight & 15) << 4);
}

void Chunk::calculateFaceAmbientOcclusionAndLight(unsigned int& ao, unsigned int& light, int x, int y, int z, int normal, uint8_t centerFaceLight) const
{
	// For each face normal, we need to check 8 neighbors around the face
	// The AO calculation depends on which direction the face is facing

	std::pair<Block, uint8_t> data[8];
	bool n[8]; // 8 neighbors around the face

	unsigned int ao0, ao1, ao2, ao3;
	unsigned int light0, light1, light2, light3;

	switch (normal)
	{
	case 0: // -X face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z + 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y, z - 1);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x - 1, y, z + 1);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z - 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		break;
	case 1: // +X face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z + 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x + 1, y, z - 1);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y, z + 1);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z - 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		break;
	case 2: // -Y face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x, y - 1, z - 1);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z - 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z + 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x, y - 1, z + 1);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		break;
	case 3: // +Y face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x, y + 1, z - 1);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z - 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z + 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x, y + 1, z + 1);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[4].second, data[1].second, data[2].second, n[4], n[1], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[3].second, data[1].second, data[0].second, n[3], n[1], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[3].second, data[6].second, data[5].second, n[3], n[6], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[4].second, data[6].second, data[7].second, n[4], n[6], n[7]);
		break;
	case 4: // -Z face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z - 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x, y - 1, z - 1);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z - 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y, z - 1);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y, z - 1);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z - 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x, y + 1, z - 1);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z - 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		break;
	case 5: // +Z face
		data[0] = getBlockAndLight_checkNeighborsTraverse(x - 1, y - 1, z + 1);
		data[1] = getBlockAndLight_checkNeighborsTraverse(x, y - 1, z + 1);
		data[2] = getBlockAndLight_checkNeighborsTraverse(x + 1, y - 1, z + 1);
		data[3] = getBlockAndLight_checkNeighborsTraverse(x - 1, y, z + 1);
		data[4] = getBlockAndLight_checkNeighborsTraverse(x + 1, y, z + 1);
		data[5] = getBlockAndLight_checkNeighborsTraverse(x - 1, y + 1, z + 1);
		data[6] = getBlockAndLight_checkNeighborsTraverse(x, y + 1, z + 1);
		data[7] = getBlockAndLight_checkNeighborsTraverse(x + 1, y + 1, z + 1);

		for (int i = 0; i < 8; i++)
		{
			n[i] = !BlockDataBase::getBlockData(data[i].first)->properties.areFacesTransparent;
		}

		calculateVertexAmbientOcclusionAndLight(ao0, light0, centerFaceLight, data[1].second, data[3].second, data[0].second, n[1], n[3], n[0]);
		calculateVertexAmbientOcclusionAndLight(ao1, light1, centerFaceLight, data[1].second, data[4].second, data[2].second, n[1], n[4], n[2]);
		calculateVertexAmbientOcclusionAndLight(ao2, light2, centerFaceLight, data[6].second, data[4].second, data[7].second, n[6], n[4], n[7]);
		calculateVertexAmbientOcclusionAndLight(ao3, light3, centerFaceLight, data[6].second, data[3].second, data[5].second, n[6], n[3], n[5]);
		break;
	}

	//
	ao = ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6);
	light = light0 | (light1 << 8) | (light2 << 16) | (light3 << 24);
}

int Chunk::getX() const
{
	return position.x;
}

int Chunk::getY() const
{
	return position.y;
}

int Chunk::getZ() const
{
	return position.z;
}

glm::ivec3 Chunk::getPosition() const
{
	return position;
}

size_t Chunk::getFaceCount() const
{
	return meshData.getOpaqueFaceCountSum() + meshData.getTransparentFaceCountSum();
}

size_t Chunk::getFaceCapacity() const
{
	return meshData.getFaceCapacity();
}

Chunk::State Chunk::getState() const
{
	return state.load(std::memory_order_acquire);
}

void Chunk::setState(State newState)
{
	state.store(newState, std::memory_order_release);
}

bool Chunk::getIsProcessing() const
{
	return processingFence.isProcessing();// || meshData.processingFence.isProcessing();
}

bool Chunk::getIsLoadedInWorld() const
{
	return isLoadedInWorld.load(std::memory_order_acquire);
}

//============================================================================
//LightNode

LightNode::LightNode(int x, int y, int z, uint8_t lightLevel, int8_t propagationSide) :
	x(x), y(y), z(z), lightLevel(lightLevel), propagationSide(propagationSide)
{
}

//============================================================================
//Int3Hasher

size_t Int3Hasher::operator()(const glm::ivec3& other) const
{
	constexpr size_t addConst = 0x9e3779b97f4a7c15;
	size_t h = (size_t)other.x + addConst;
	h ^= (size_t)other.y + addConst + (h << 6) + (h >> 2);
	h ^= (size_t)other.z + addConst + (h << 6) + (h >> 2);
	return h;
}

//============================================================================