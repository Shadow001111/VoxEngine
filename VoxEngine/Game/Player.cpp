#include "Player.h"

Player::Player(const glm::vec3& position, float yaw, float pitch) :
	transform(position, yaw, pitch), previousTransform(transform),
	camera(position, yaw, pitch, glm::radians(90.0f), 1.0f, 0.1f, 128.0f)
{
}

void Player::updatePreviousTransform()
{
	previousTransform = transform;
}

void Player::update(const PlayerInput& input, float deltaTime)
{
	// Position
    {
		const float cameraSpeed = (input.sprint ? 4.0f : 1.0f) * (15.0f * deltaTime);

        float leftRight = input.moveRight - input.moveLeft;
        float forwardBackward = input.moveForward - input.moveBackward;
        float worldUpDown = input.moveUp - input.moveDown;

        glm::vec3 movementVector = glm::vec3(0.0f);

        movementVector += camera.getRight() * leftRight;
        movementVector += camera.getForward() * forwardBackward;
        movementVector.y += worldUpDown;

        if (glm::length(movementVector) > 0.0f)
        {
            movementVector = glm::normalize(movementVector) * cameraSpeed;
            move(movementVector);
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

glm::vec3 Player::getPosition() const
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
