#pragma once
#include "Core/Transform.h"
#include <cstdint>

class World;

class Entity
{
	struct MoveResult
	{
		bool collided = false;
		int collisionAxis = -1;
		double stepDirection = 0.0;
		double collisionTime = 0.0;
		glm::dvec3 startPos;
		glm::dvec3 ray;
		glm::dvec3 actualMovement;
		glm::ivec3 collisionPos;
	};
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
	void moveAndCheckForCollisions(const glm::dvec3& dpos);

	void moveAndCheckForCollisions_SpeedSafe(const glm::dvec3& dpos);

	void moveAndCheckForCollisions_DDA(const glm::dvec3& dpos);
	bool checkCollisionTiles(const glm::dvec3& pos, const glm::dvec3& move, const glm::ivec3& checkPos, double distance, int checkAxis) const;

	bool isAnyBlocksSolidInside(double sign, int axis, glm::ivec3& outPos) const;
	bool isAnyBlocksSolidAt(const glm::ivec3& min, const glm::ivec3& max, glm::ivec3& outPos) const;
	bool isBlockSolidAt(const glm::ivec3 pos) const;
};

