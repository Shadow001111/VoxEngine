#include "Entity.h"
#include "../World.h"

#include "Game/DataPackManagment/AssetRegistry.h"

constexpr double GRAVITY = -32.0;
constexpr double MIN_Y_VELOCITY = -78.4;

Entity::Id Entity::globalEntityId = 0;
World* Entity::world = nullptr;

Entity::Entity(const glm::dvec3& position, float yaw, float pitch, const glm::dvec3& velocity, const glm::dvec3& size, bool hasGravity) :
	id(globalEntityId++),
	transform(position, yaw, pitch), previousTransform(transform),
	velocity(velocity), size(size),
	hasGravity(hasGravity)
{
}

void Entity::update(double deltaTime)
{
	previousTransform = transform;
	onGround = false;

	// Apply gravity
	if (hasGravity)
	{
		velocity.y += GRAVITY * deltaTime;
		velocity.y = glm::max(velocity.y, MIN_Y_VELOCITY);
	}

	// Move
	//moveAndCheckForCollisions_DDA(velocity * deltaTime);
	transform.position += velocity * deltaTime;
}

void Entity::moveAndCheckForCollisions_DDA(const glm::dvec3& dpos)
{
	glm::dvec3 move = dpos;
	const glm::dvec3 halfSize = size * 0.5;

	constexpr double COLLISION_SEPARATION = 1e-6;

	for (int iteration = 0; iteration < 3; iteration++)
	{
		if (glm::length(move) < 1e-6)
		{
			return;
		}

		glm::ivec3 step = glm::ivec3(glm::sign(move));

		glm::dvec3 deltaDist;
		for (int i = 0; i < 3; i++)
		{
			if (abs(move[i]) > 1e-6)
			{
				deltaDist[i] = abs(1.0 / move[i]);
			}
			else
			{
				deltaDist[i] = std::numeric_limits<double>::max();
			}
		}

		glm::dvec3 startEdge = transform.position + halfSize * glm::dvec3(step);

		glm::ivec3 mapPos = glm::floor(startEdge);

		glm::dvec3 sideDist;
		for (int i = 0; i < 3; i++)
		{
			if (abs(move[i]) > 1e-6)
			{
				sideDist[i] = ((mapPos[i] + (step[i] > 0)) - startEdge[i]) / move[i];
			}
			else
			{
				sideDist[i] = std::numeric_limits<double>::max();
			}
		}

		double totalDist = 0.0;
		double maxDist = glm::length(move);
		bool hit = false;
		int hitAxis = -1;
		glm::ivec3 hitMapPos;

		while (true)
		{
			int moveAxis;
			if (sideDist.x < sideDist.y && sideDist.x < sideDist.z)
			{
				moveAxis = 0;
			}
			else if (sideDist.y < sideDist.z)
			{
				moveAxis = 1;
			}
			else
			{
				moveAxis = 2;
			}

			totalDist = abs(sideDist[moveAxis]) * glm::length(move);
			sideDist[moveAxis] += deltaDist[moveAxis];
			mapPos[moveAxis] += step[moveAxis];
			hitAxis = moveAxis;

			if (totalDist >= maxDist)
			{
				break;
			}

			if (checkCollisionTiles(transform.position, move, mapPos, totalDist, hitAxis))
			{
				transform.position += move * (totalDist / maxDist);
				hit = true;
				hitMapPos = mapPos;
				break;
			}
		}

		if (hit)
		{
			if (step[hitAxis] > 0)
			{
				transform.position[hitAxis] = hitMapPos[hitAxis] - halfSize[hitAxis] - COLLISION_SEPARATION;
			}
			else
			{
				transform.position[hitAxis] = (hitMapPos[hitAxis] + 1) + halfSize[hitAxis] + COLLISION_SEPARATION;
			}
			move[hitAxis] = 0.0;
			velocity[hitAxis] = 0.0;
			if (hitAxis == 1 && step[hitAxis] < 0)  // Y-axis collision moving downward
			{
				onGround = true;
			}
		}
		else
		{
			transform.position += move;
			return;
		}
	}
}

bool Entity::checkCollisionTiles(const glm::dvec3& pos, const glm::dvec3& move, const glm::ivec3& checkPos, double distance, int checkAxis) const
{
	glm::dvec3 halfSize = size * 0.5;
	glm::dvec3 moveDir = glm::normalize(move);

	// Calculate where the player will be at this distance
	glm::dvec3 projectedPos = pos + moveDir * distance;

	// Determine which tiles to check based on which axis we're hitting
	glm::ivec3 minBounds, maxBounds;

	if (checkAxis == 0)
	{   // X-axis collision
		minBounds.x = maxBounds.x = checkPos.x;
		minBounds.y = (int)glm::floor(projectedPos.y - halfSize.y);
		maxBounds.y = (int)glm::floor(projectedPos.y + halfSize.y);
		minBounds.z = (int)glm::floor(projectedPos.z - halfSize.z);
		maxBounds.z = (int)glm::floor(projectedPos.z + halfSize.z);
	}
	else if (checkAxis == 1)
	{   // Y-axis collision
		minBounds.x = (int)glm::floor(projectedPos.x - halfSize.x);
		maxBounds.x = (int)glm::floor(projectedPos.x + halfSize.x);
		minBounds.y = maxBounds.y = checkPos.y;
		minBounds.z = (int)glm::floor(projectedPos.z - halfSize.z);
		maxBounds.z = (int)glm::floor(projectedPos.z + halfSize.z);
	}
	else
	{   // Z-axis collision
		minBounds.x = (int)glm::floor(projectedPos.x - halfSize.x);
		maxBounds.x = (int)glm::floor(projectedPos.x + halfSize.x);
		minBounds.y = (int)glm::floor(projectedPos.y - halfSize.y);
		maxBounds.y = (int)glm::floor(projectedPos.y + halfSize.y);
		minBounds.z = maxBounds.z = checkPos.z;
	}

	glm::ivec3 collisionPos;
	return isAnyBlocksSolidAt(minBounds, maxBounds, collisionPos);
}

bool Entity::isAnyBlocksSolidAt(const glm::ivec3& min, const glm::ivec3& max, glm::ivec3& outPos) const
{
	glm::ivec3 samplePos;
	for (samplePos.x = min.x; samplePos.x <= max.x; samplePos.x++)
	for (samplePos.y = min.y; samplePos.y <= max.y; samplePos.y++)
	for (samplePos.z = min.z; samplePos.z <= max.z; samplePos.z++)
	{
		if (isBlockSolidAt(samplePos))
		{
			outPos = samplePos;
			return true;
		}
	}
	return false;
}

bool Entity::isBlockSolidAt(const glm::ivec3 pos) const
{
	auto block = world->getBlockAt(pos);
	if (!block.has_value())
	{
		return false;
	}

	const auto* blockData = AssetRegistry::getBlockData(block.value());
	return blockData ? blockData->hasFaces : false;
}
