#include "Player.h"

#include <iostream>

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
	Entity(position, yaw, pitch, glm::dvec3(0.0), glm::dvec3(0.6, 1.7, 0.6), true),
	camera(position, yaw, pitch, glm::radians(90.0f), 1.0f, 0.1f, 1.0f)
{
}

void Player::update(double deltaTime)
{
	Entity::update(deltaTime);

	// Position and velocity
	{
		// Apply friction
		double friction = pow(0.2, deltaTime);
		velocity.x *= friction;
		velocity.z *= friction;

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

			// Get acceleration
			constexpr double MAX_SPEED = 10.0;
			constexpr double MAX_ACCELERATION = 50.0;
			double currentSpeed = glm::dot(velocity, wishDir);

			double acceleration = fmax(0.0, fmin(MAX_ACCELERATION * deltaTime, MAX_SPEED - currentSpeed));

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

	// Reset input
	resetInput();
}

void Player::resetInput()
{
	input.moveForward = false;
	input.moveBackward = false;
	input.moveLeft = false;
	input.moveRight = false;
	input.moveUp = false;
	input.moveDown = false;
	input.sprint = false;

	for (int i = 0; i <= 9; i++)
	{
		input.numbers[i] = false;
	}

	input.leftMousePressed = false;
	input.rightMousePressed = false;

	input.mouseDelta = glm::vec2(0.0f);
}

void Player::interpolateCameraTransform(float factor)
{
    Transform interpolatedTransform = previousTransform.interpolate(transform, factor);
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

glm::dvec3 Player::getPosition() const
{
	return transform.position;
}

float Player::getYaw() const
{
	return transform.yaw;
}

float Player::getPitch() const
{
	return transform.pitch;
}

Transform Player::getTransform() const
{
	return transform;
}

Transform Player::getPreviousTransform() const
{
	return previousTransform;
}

Camera& Player::getCamera()
{
	return camera;
}

Block Player::getSelectedItem() const
{
	return hotbar[selectedItemIndex];
}
