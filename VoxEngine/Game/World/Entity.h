#pragma once
#include "Core/Transform.h"
#include <cstdint>

class World;

class Entity
{
public:
	using Id = uint64_t;
private:
	static Id globalEntityId;
	Id id = 0;
protected:
	Transform transform;
	Transform previousTransform;

	glm::dvec3 velocity;
	glm::dvec3 size; // Half - extents of the entity's bounding box

	bool hasGravity = false;

	bool onGround = false;
public:
	static World* world;

	Entity(const glm::dvec3& position, float yaw, float pitch, const glm::dvec3& velocity, const glm::dvec3& size, bool hasGravity);
	virtual ~Entity() = default;

	// Update entity physics and logic
	virtual void update(double deltaTime);

	//
	Id getId() const noexcept { return id; }
private:
	bool isBlockSolidAt(const glm::ivec3 pos) const;
};

