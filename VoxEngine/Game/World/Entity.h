#pragma once
#include "Core/Transform.h"
#include <cstdint>

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

	glm::vec3 velocity;
	glm::vec3 size; // Half - extents of the entity's bounding box

	bool hasGravity = false;
public:
	Entity(const glm::vec3& position, float yaw, float pitch, const glm::vec3& velocity, const glm::vec3& size, bool hasGravity);
	virtual ~Entity() = default;

	// Update entity physics and logic
	virtual void update(float deltaTime);

	//
	Id getId() const noexcept { return id; }
};

