#include "Entity.h"
#include "../World.h"

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

	// Apply gravity
	if (hasGravity)
	{
		constexpr double GRAVITY = -32.0;
		velocity.y += GRAVITY * deltaTime;
		velocity.y = glm::max(velocity.y, -78.4);
	}

	// TODO: Use DDA for movement. It should be better.
	
	// Move
	onGround = false;
	for (int axis = 0; axis < 3; axis++)
	{
		float sign = copysign(1.0, velocity[axis]);
		float add = sign < 0.0;

		transform.position[axis] += velocity[axis] * deltaTime;

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

bool Entity::isBlockSolidAt(const glm::ivec3 pos) const
{
	auto block = world->getBlockAt(pos);
	return block.has_value() && BlockDataBase::getBlockData(block.value())->properties.hasFaces;
}
