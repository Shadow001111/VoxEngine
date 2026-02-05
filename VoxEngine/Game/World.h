#pragma once
#include "WorldImpl/WorldChunkManager.h"
#include "WorldImpl/WorldRenderer.h"

#include "World/ChunkPool.h"
#include "World/Entity.h"

class World
{
	WorldChunkManager chunkManager;
	WorldRenderer renderer;
public:
	struct DebugData
	{
		size_t loadedChunksCount = 0;

		size_t totalChunkFaceCount = 0;
		size_t totalChunkFaceCapacity = 0;
		size_t totalChunkFaceCapacityInBytes = 0;

		WorldRenderer::RenderStats renderStats;
	};
private:
	// Settings
	int chunkLoadingDistance = 0;

	// Debug
	mutable DebugData debugData;

	// Entities
	robin_hood::unordered_flat_map<Entity::Id, std::unique_ptr<Entity>> entities;

	// Time
	float appTime = 0.0;
	size_t worldTime = 0;
	float dayNightCycleValue = 0.0f; // 1 day; 0 night
	float skyLightSub = 0.0f;
	static constexpr int TICKS_PER_24_HOURS = 24000;
public:
	World();
	~World() = default;

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;
public:
	void preparation();

	void loadChunks(const glm::dvec3& playerPos);

	void update(float deltaTime);

	void sendChunkMeshesToGPU();

	void render(const Camera& camera, const FrameBuffer& FBO, const RaycastResult& raycast);

	RaycastResult raycast(const glm::dvec3& origin, const glm::dvec3& direction, float maxDistance = 100.0f) const;

	template<typename T, typename... Args>
	T* createEntity(Args&&... args) {
		static_assert(std::is_base_of<Entity, T>::value, "T must derive from Entity");
		static_assert(!std::is_same_v<Entity, T>, "T mustn't be Entity");

		std::unique_ptr<T> e = std::make_unique<T>(std::forward<Args>(args)...);
		T* raw = e.get();
		entities.emplace(e->getId(), std::move(e));
		return raw;
	}

	// Debug
	void rebuildAllChunkMeshes();
	void debugMethod();

	const DebugData& getDebugData(bool updateIntense) const;

	bool placeBlock(const RaycastResult& raycast, BlockId block);
	bool breakBlock(const RaycastResult& raycast);
	void updateBlockAt(const glm::ivec3& worldPos, BlockId block);
	std::optional<BlockId> getBlockAt(const glm::ivec3& globalPosition) const;

	void setChunkLoadingDistance(int loadingDistanceInChunks);

	void setAppTime(float time);
};

