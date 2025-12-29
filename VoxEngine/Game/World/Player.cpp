#include "Player.h"

#include "../World.h"
#include "Game/DataPackManagment/AssetRegistry.h"

glm::dvec3 makeVectorFlatNormalized(const glm::dvec3& vec)
{
	glm::dvec3 flat = vec;
	flat.y = 0.0;

	double squaredLength = glm::dot(flat, flat);
	if (squaredLength == 0.0)
	{
		return flat;
	}

	return flat / sqrt(squaredLength);
}

Player::Player(const glm::dvec3& position, float yaw, float pitch) :
	Entity(position, yaw, pitch, glm::dvec3(0.0), glm::dvec3(0.6, 1.7, 0.6) * 0.5, true),
	camera(position, yaw, pitch, glm::radians(90.0f), 1.0f, 0.1f, 1.0f)
{
	setGameMode(GameMode::Fly);

	for (int i = 0; i < 9; i++)
	{
		hotbar[i].id = i;
		hotbar[i].count = 1;
		if (AssetRegistry::getItemData(i) == nullptr)
		{
			break;
		}
	}
}

void Player::update(double deltaTime)
{
	Entity::update(deltaTime);

	if (gameMode == GameMode::Normal)
	{
		// Jump
		if (onGround && input.jump)
		{
			velocity.y = 10.0;
			onGround = false;
		}

		// Position and velocity
		{
			double friction, maxSpeed, maxAcceleration;
			getMovingValues(friction, maxSpeed, maxAcceleration);

			// Apply friction
			{
				double frictionForce = friction * deltaTime;
				glm::dvec3 flatVelocity = glm::dvec3(velocity.x, 0.0, velocity.z);
				if (frictionForce > glm::length(flatVelocity))
				{
					velocity.x = 0.0;
					velocity.z = 0.0;
				}
				else
				{
					velocity -= glm::normalize(flatVelocity) * frictionForce;
				}
			}

			// Get flat vectors
			glm::dvec3 right = makeVectorFlatNormalized(camera.getRight());
			glm::dvec3 forward = makeVectorFlatNormalized(camera.getForward());

			// Get wishDir
			double leftRight = input.moveRight - input.moveLeft;
			double forwardBackward = input.moveForward - input.moveBackward;
			glm::dvec3 wishDir = right * leftRight + forward * forwardBackward;

			if (glm::dot(wishDir, wishDir) > 0.0)
			{
				wishDir = glm::normalize(wishDir);

				double currentSpeed = glm::dot(velocity, wishDir);

				double acceleration = fmax(0.0, fmin(maxAcceleration * deltaTime, maxSpeed - currentSpeed));

				// Apply acceleration
				velocity += wishDir * acceleration;
			}
		}
	}
	else if (gameMode == GameMode::Fly)
	{
		double friction = 50.0, maxSpeed = 100.0, maxAcceleration = 100.0;

		// Apply friction
		{
			double frictionForce = friction * deltaTime;
			if (frictionForce > glm::length(velocity))
			{
				velocity = { 0.0, 0.0, 0.0 };
			}
			else
			{
				velocity -= glm::normalize(velocity) * frictionForce;
			}
		}

		// Get flat vectors
		glm::dvec3 right = camera.getRight();
		glm::dvec3 forward = camera.getForward();

		// Get wishDir
		double leftRight = input.moveRight - input.moveLeft;
		double forwardBackward = input.moveForward - input.moveBackward;

		glm::dvec3 wishDir = right * leftRight + forward * forwardBackward;
		if (input.jump)
		{
			wishDir += glm::dvec3(0.0f, 1.0f, 0.0f);
		}

		if (glm::dot(wishDir, wishDir) > 0.0)
		{
			wishDir = glm::normalize(wishDir);

			double currentSpeed = glm::dot(velocity, wishDir);

			double acceleration = fmax(0.0, fmin(maxAcceleration * deltaTime, maxSpeed - currentSpeed));

			// Apply acceleration
			velocity += wishDir * acceleration;
		}
	}

	// Rotation
	{
		const float mouseSensitivity = 0.002f;
		rotate(-input.mouseDelta.x * mouseSensitivity, -input.mouseDelta.y * mouseSensitivity);
	}

	// Selecting item
	for (int i = 0; i < PLAYER_HOTBAR_SIZE; i++)
	{
		if (input.numbers[i + 1])
		{
			selectedItemIndex = i;
		}
	}

	// Raycast
	raycastResult = world->raycast(camera.getPosition(), camera.getForward(), 16.0f);
	if (raycastResult.hit)
	{
		if (input.leftMouseClicked)
		{
			input.leftMouseClicked = false;

			const auto& item = hotbar[selectedItemIndex];
			const auto* itemData = AssetRegistry::getItemData(item.id);
			if (itemData && itemData->hasBlockPlaceable)
			{
				world->placeBlock(raycastResult, itemData->blockPlaceableId);
			}
		}
		if (input.rightMouseClicked)
		{
			input.rightMouseClicked = false;

			world->breakBlock(raycastResult);
		}
	}

	// Reset input
	resetInput();
}

void Player::resetInput()
{
	input.moveForward = false;
	input.moveBackward = false;
	input.moveLeft = false;
	input.moveRight = false;
	input.jump = false;
	input.crouch = false;
	input.sprint = false;

	for (int i = 0; i <= 9; i++)
	{
		input.numbers[i] = false;
	}

	input.leftMousePressed = false;
	input.rightMousePressed = false;

	input.leftMouseClicked = false;
	input.rightMouseClicked = false;

	input.mouseDelta = glm::vec2(0.0f);
}

void Player::getMovingValues(double& friction, double& maxSpeed, double& maxAcceleration) const
{
	if (onGround)
	{
		bool moveAny = input.moveForward || input.moveBackward || input.moveLeft || input.moveRight;
		if (input.sprint && moveAny)
		{
			friction = 10.0;
			maxSpeed = 1000.0;
			maxAcceleration = 100.0;
		}
		else
		{
			friction = 10.0;
			maxSpeed = 5.0;
			maxAcceleration = 50.0;
		}
	}
	else
	{
		friction = 1.0;
		maxSpeed = 3.0;
		maxAcceleration = 25.0;
	}
}

void Player::interpolateCameraTransform(float factor)
{
    Transform interpolatedTransform = previousTransform.interpolate(transform, factor);
	interpolatedTransform.position.y += 1.7 * 0.5 - 0.2;
	camera.setTransform(interpolatedTransform);
}

void Player::setPosition(const glm::vec3& position)
{
	transform.position = position;
}

void Player::setYaw(float yaw)
{
	transform.yaw = yaw;
}

void Player::setPitch(float pitch)
{
	transform.pitch = glm::clamp(pitch, -1.5707f, 1.5707f);
}

void Player::setYawPitch(float yaw, float pitch)
{
	transform.yaw = yaw;
	transform.pitch = glm::clamp(pitch, -1.5707f, 1.5707f);
}

void Player::setTransform(const Transform& transform)
{
	this->transform = transform;
	this->transform.pitch = glm::clamp(this->transform.pitch, -1.5707f, 1.5707f);
}

void Player::setGameMode(GameMode gameMode)
{
	this->gameMode = gameMode;
	if (gameMode == GameMode::Normal)
	{
		Entity::hasGravity = true;
	}
	else if (gameMode == GameMode::Fly)
	{
		Entity::hasGravity = false;
	}
}

void Player::move(const glm::vec3& delta)
{
	transform.position += delta;
}

void Player::rotate(float deltaYaw, float deltaPitch)
{
	transform.yaw += deltaYaw;
	transform.pitch += deltaPitch;
	transform.pitch = glm::clamp(transform.pitch, -1.5707f, 1.5707f);
}
