#include "Entity.h"

Entity::Id Entity::globalEntityId = 0;

Entity::Entity(const glm::vec3& position, float yaw, float pitch, const glm::vec3& velocity, const glm::vec3& size, bool hasGravity) :
	id(globalEntityId++),
	transform(position, yaw, pitch), previousTransform(transform),
	velocity(velocity), size(size),
	hasGravity(hasGravity)
{
}

void Entity::update(float deltaTime)
{
	previousTransform = transform;

	// Apply gravity
	constexpr float GRAVITY = -22.0f;

	if (hasGravity)
	{
		velocity.y += GRAVITY * deltaTime;
		velocity.y = glm::max(velocity.y, -78.4f);
	}
	
	// Apply movement
	transform.position += velocity * deltaTime;
}
