#include "ChunkMeshAllocator.h"

#include <iostream>

#include "Core/Assert.h"
#include "Game/TracyProfiler.h"

#include "Game/ProfileCategories.h"

static constexpr std::array<const char*, 4> kLayerDebugNames = {
	"processAlignedOpaqueMeshRequests",
	"processAlignedTranslucentMeshRequests",
	"processUnalignedOpaqueMeshRequests",
	"processUnalignedTranslucentMeshRequests"
};

ChunkMeshAllocator::ChunkMeshAllocator()
{
	// Shared quad geometry (CCW)
	const float vertices[8] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	vbo.create(GL_ARRAY_BUFFER);
	vbo.allocateStorage(sizeof(vertices), 0, vertices);

	// Initialise each MeshAllocator with its layer-specific config
	for (int i = 0; i < (int)MeshLayer::Count; i++)
	{
		auto layer = static_cast<MeshLayer>(i);
		auto& ma = meshAllocators[i];

		ProcessorConfig cfg = {
			[this, layer]() { configureInstanceVBO(layer); },

			[layer](ChunkMeshFaceStorage* m) { return m->getFaceCount(layer); },

			[layer](ChunkMeshFaceStorage* m) {
				return m->readFlag(ChunkMeshFaceStorage::createdFlag(layer));
			},

			[layer](ChunkMeshFaceStorage* m) -> ChunkMeshFaceStorage::Block& {
				return m->getFacesBlock(layer);
			},

			[layer](ChunkMeshFaceStorage* m, bool created) {
				m->setFlag(ChunkMeshFaceStorage::createdFlag(layer), created);
			},

			[layer](ChunkMeshFaceStorage* m, ChunkMeshFaceStorage::Block block) {
				m->getFacesBlock(layer) = block;
			},

			faceStructSize(layer),
			kLayerDebugNames[i]
		};
		ma.init(cfg);

		// Bind shared quad VBO into slot 0 and configure the instance VBO.
		ma.vao.bindVertexBuffer(0, vbo.getID(), 0, 2 * sizeof(float));
		ma.vao.enableAttribute(0);
		ma.vao.setFloatAttribute(0, 2, 0, 0);

		configureInstanceVBO(layer);
	}
}

void ChunkMeshAllocator::configureInstanceVBO(MeshLayer layer)
{
	auto& ma = meshAllocators[static_cast<uint8_t>(layer)];
	ma.vao.bindVertexBuffer(1, ma.instanceVBO.getID(), 0, faceStructSize(layer));

	if (isAligned(layer))
	{
		ma.vao.enableAttribute(1);
		ma.vao.setIntAttribute(1, 2, 0, 1, GL_UNSIGNED_INT);
		ma.vao.setAttributeDivisor(1, 1);
	}
	else
	{
		// Data
		ma.vao.enableAttribute(1);
		ma.vao.setIntAttribute(1, 4, 0, 1, GL_UNSIGNED_INT);
		ma.vao.setAttributeDivisor(1, 1);

		ma.vao.enableAttribute(2);
		ma.vao.setIntAttribute(2, 4, 4 * sizeof(int), 1, GL_UNSIGNED_INT);
		ma.vao.setAttributeDivisor(2, 1);

		ma.vao.enableAttribute(3);
		ma.vao.setIntAttribute(3, 4, 8 * sizeof(int), 1, GL_UNSIGNED_INT);
		ma.vao.setAttributeDivisor(3, 1);
	}
}

ChunkMeshAllocator& ChunkMeshAllocator::getInstance()
{
	static ChunkMeshAllocator instance;
	return instance;
}

void ChunkMeshAllocator::processMeshAllocationRequests(const LayerRequests& requests)
{
	for (int i = 0; i < (int)MeshLayer::Count; i++)
	{
		meshAllocators[i].processMeshRequests(requests[i]);
	}
}

ChunkMeshAllocator::MeshAllocator::MeshAllocator() :
	blockAllocator(0)
{
	vao.create();
	instanceVBO.create(GL_ARRAY_BUFFER);
}

void ChunkMeshAllocator::MeshAllocator::init(const ProcessorConfig& config)
{
	this->config = config;
}

void ChunkMeshAllocator::MeshAllocator::processMeshRequests(const DynamicArray<ChunkMeshFaceStorage*>& meshRequests)
{
	// Store old capacity
	size_t oldCapacity = blockAllocator.getCapacity();

	// Free blocks
	{
		TRACY_SCOPE_NC("Free blocks", ProfileCategory::ChunkMesh);
		for (ChunkMeshFaceStorage* chunkMesh : meshRequests)
		{
			if (!config.isCreated(chunkMesh))
			{
				continue;
			}

			ASSERT(blockAllocator.free(config.getAllocatedBlock(chunkMesh)));
		}
	}

	// Allocate blocks
	size_t stopIndex = 0;
	{
		TRACY_SCOPE_NC("Allocate blocks", ProfileCategory::ChunkMesh);
		for (ChunkMeshFaceStorage* chunkMesh : meshRequests)
		{
			auto faceCount = config.getFaceCount(chunkMesh);

			auto result = blockAllocator.allocate(faceCount);
			if (!result.has_value())
			{
				break;
			}

			config.setCreated(chunkMesh, true);
			config.setAllocatedBlock(chunkMesh, result.value());
			stopIndex++;
		}
	}

	if (stopIndex == meshRequests.size())
	{
		return;
	}

	// Allocate more memory
	size_t newCapacity;
	{
		TRACY_SCOPE_NC("Calculate new capacity", ProfileCategory::ChunkMesh);
		newCapacity = blockAllocator.getLastAllocatedBlockEnd();
		for (size_t i = stopIndex; i < meshRequests.size(); i++)
		{
			newCapacity += config.getFaceCount(meshRequests[i]);
		}
		newCapacity += (newCapacity >> 1); // *= 1.5
	}

	blockAllocator.setCapacity(newCapacity);
	if (oldCapacity == 0)
	{
		TRACY_SCOPE_NC("Allocate storage", ProfileCategory::ChunkMesh);
		instanceVBO.allocateStorage(newCapacity * config.faceSize, INSTANCE_VBO_FLAGS);
	}
	else
	{
		TRACY_SCOPE_NC("Allocate storage and copy old data", ProfileCategory::ChunkMesh);

		// Create new buffer
		ImmutableBuffer newBuffer;
		newBuffer.create(GL_ARRAY_BUFFER);
		newBuffer.allocateStorage(newCapacity * config.faceSize, INSTANCE_VBO_FLAGS);

		// Copy data to a new buffer
		auto boundsOpt = blockAllocator.getAllocatedRangeBounds();

		if (boundsOpt.has_value())
		{
			auto bounds = boundsOpt.value();
			newBuffer.copyRangeFrom(
				instanceVBO,
				bounds.first * config.faceSize,
				bounds.first * config.faceSize,
				bounds.second * config.faceSize
			);
		}

		// Replace old with new buffer
		instanceVBO = std::move(newBuffer);
		config.configureVBO();
	}

	// Allocate the rest of blocks
	{
		TRACY_SCOPE_NC("Allocate blocks", ProfileCategory::ChunkMesh);

		for (size_t i = stopIndex; i < meshRequests.size(); i++)
		{
			ChunkMeshFaceStorage* chunkMesh = meshRequests[i];
			auto faceCount = config.getFaceCount(chunkMesh);

			auto result = blockAllocator.allocate(faceCount);
			if (!result.has_value())
			{
				std::cerr << "[ChunkMeshAllocator]: " << config.debugName << ": mesh wasn't created.\n";
				break;
			}

			config.setCreated(chunkMesh, true);
			config.setAllocatedBlock(chunkMesh, result.value());
		}
	}
}
