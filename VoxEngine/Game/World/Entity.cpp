#include "Entity.h"
#include "../World.h"
#include "Chunk/BlockRegistry.h"

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
		constexpr double GRAVITY = -32.0;
		velocity.y += GRAVITY * deltaTime;
		velocity.y = glm::max(velocity.y, -78.4);
	}

	// Move
	moveAndCheckForCollisions(velocity * deltaTime);
}

void Entity::moveAndCheckForCollisions(const glm::dvec3& dpos)
{
	for (int axis = 0; axis < 3; axis++)
	{
		double sign = copysign(1.0, dpos[axis]);
		double add = sign < 0.0;

		transform.position[axis] += dpos[axis];

		int minX, maxX;
		int minY, maxY;
		int minZ, maxZ;

		if (axis == 0)
		{
			minX = maxX = floor(transform.position.x + sign * size.x);

			minY = floor(transform.position.y - size.y);
			maxY = floor(transform.position.y + size.y);

			minZ = floor(transform.position.z - size.z);
			maxZ = floor(transform.position.z + size.z);
		}
		else if (axis == 1)
		{
			minX = floor(transform.position.x - size.x);
			maxX = floor(transform.position.x + size.x);

			minY = maxY = floor(transform.position.y + sign * size.y);

			minZ = floor(transform.position.z - size.z);
			maxZ = floor(transform.position.z + size.z);
		}
		else
		{
			minX = floor(transform.position.x - size.x);
			maxX = floor(transform.position.x + size.x);

			minY = floor(transform.position.y - size.y);
			maxY = floor(transform.position.y + size.y);

			minZ = maxZ = floor(transform.position.z + sign * size.z);
		}

		bool foundCollision = false;
		for (int x = minX; x <= maxX && !foundCollision; x++)
		{
			for (int y = minY; y <= maxY && !foundCollision; y++)
			{
				for (int z = minZ; z <= maxZ && !foundCollision; z++)
				{
					glm::ivec3 blockPos = { x, y, z };
					if (isBlockSolidAt(blockPos))
					{
						transform.position[axis] = (double)blockPos[axis] + add - sign * (size[axis] + 1e-6);
						velocity[axis] = 0.0;
						foundCollision = true;
						break;
					}
				}
			}
		}

		if (axis == 1 && foundCollision && sign < 0.0)
		{
			onGround = true;
		}
	}
}

void Entity::moveAndCheckForCollisions_DDA(const glm::dvec3& dpos)
{
	if (glm::length(dpos) < 1e-6)
	{
		return;
	}

	glm::dvec3 moveVector = dpos;
	for (int iteration = 0; iteration < 3; iteration++)
	{
		const double maxDistance = glm::length(moveVector);

		const glm::dvec3 dir = glm::normalize(moveVector);
		const glm::dvec3 origin = transform.position;

		glm::ivec3 blockPos = glm::floor(origin);
		const glm::ivec3 step = glm::sign(dir);

		glm::dvec3 tDelta, tMax;
		for (int i = 0; i < 3; i++)
		{
			if (std::abs(dir[i]) < 0.0001)
			{
				tDelta[i] = std::numeric_limits<double>::max();
				tMax[i] = std::numeric_limits<double>::max();
			}
			else
			{
				double invDir = 1.0 / dir[i];
				double delta = abs(invDir);
				tDelta[i] = delta;
				if (step[i] > 0)
				{
					tMax[i] = (1.0 - glm::fract(origin[i])) * delta;
				}
				else
				{
					tMax[i] = glm::fract(origin[i]) * delta;
				}
			}
		}

		bool collisionFound = false;
		double distanceTraveled = 0.0;
		while (distanceTraveled < maxDistance)
		{
			int axis;
			if (tMax.x < tMax.y)
			{
				if (tMax.x < tMax.z)
					axis = 0;
				else
					axis = 2;
			}
			else
			{
				if (tMax.y < tMax.z)
					axis = 1;
				else
					axis = 2;
			}

			distanceTraveled = tMax[axis];
			tMax[axis] += tDelta[axis];
			blockPos[axis] += step[axis];

			// Choose axes for collision check
			int minX, maxX;
			int minY, maxY;
			int minZ, maxZ;

			const glm::dvec3 currentPos = origin + dir * distanceTraveled;
			if (axis == 0)
			{
				minX = maxX = floor(currentPos.x + step[axis] * size.x);

				minY = floor(currentPos.y - size.y);
				maxY = floor(currentPos.y + size.y);

				minZ = floor(currentPos.z - size.z);
				maxZ = floor(currentPos.z + size.z);
			}
			else if (axis == 1)
			{
				minX = floor(currentPos.x - size.x);
				maxX = floor(currentPos.x + size.x);

				minY = maxY = floor(currentPos.y + step[axis] * size.y);

				minZ = floor(currentPos.z - size.z);
				maxZ = floor(currentPos.z + size.z);
			}
			else
			{
				minX = floor(currentPos.x - size.x);
				maxX = floor(currentPos.x + size.x);

				minY = floor(currentPos.y - size.y);
				maxY = floor(currentPos.y + size.y);

				minZ = maxZ = floor(currentPos.z + step[axis] * size.z);
			}

			//
			glm::ivec3 collisionPos;
			if (isAnyBlocksSolidAt(glm::ivec3(minX, minY, minZ), glm::ivec3(maxX, maxY, maxZ), collisionPos))
			{
				double add = step[axis] < 0.0;
				transform.position = currentPos;
				transform.position[axis] = (double)collisionPos[axis] + add - step[axis] * (size[axis] + 1e-6);

				velocity[axis] = 0.0;

				moveVector -= dir * distanceTraveled;
				moveVector[axis] = 0.0;

				if (axis == 1 && step[axis] < 0.0)
				{
					onGround = true;
				}
				collisionFound = true;
				break;
			}
		}
		if (!collisionFound)
		{
			transform.position = origin + moveVector;
			return;
		}
	}
}

bool Entity::isAnyBlocksSolidAt(const glm::ivec3& min, const glm::ivec3& max, glm::ivec3& outPos)
{
	for (int x = min.x; x <= max.x; x++)
	for (int y = min.y; y <= max.y; y++)
	for (int z = min.z; z <= max.z; z++)
	{
		glm::ivec3 samplePos = { x, y, z };
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
	return block.has_value() && BlockDataBase::getBlockData(block.value())->properties.hasFaces;
}
