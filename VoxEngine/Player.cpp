#include "Player.h"

Player::Player(const glm::vec3& position, float yaw, float pitch) :
	transform(position, yaw, pitch), previousTransform(transform),
	camera(position, yaw, pitch, glm::radians(90.0f), 1.0f, 0.1f, 128.0f)
{
}

void Player::update(const WindowManager& wnd, float deltaTime, glm::vec2& lastMousePos)
{
	previousTransform = transform;

    // Position
    {
		bool sprint = wnd.isKeyPressed(GLFW_KEY_LEFT_SHIFT);

        const float cameraSpeed = (sprint ? 2.0f : 1.0f) * (15.0f * deltaTime);

        float leftRight = wnd.isKeyPressed(GLFW_KEY_D) - wnd.isKeyPressed(GLFW_KEY_A);
        float forwardBackward = wnd.isKeyPressed(GLFW_KEY_W) - wnd.isKeyPressed(GLFW_KEY_S);
        float worldUpDown = wnd.isKeyPressed(GLFW_KEY_SPACE) - wnd.isKeyPressed(GLFW_KEY_LEFT_CONTROL);

        glm::vec3 movementVector = glm::vec3(0.0f);

        movementVector += camera.getRight() * leftRight;
        movementVector += camera.getFront() * forwardBackward;
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

        float mouseX, mouseY;
        wnd.getMousePos(mouseX, mouseY);

        float offsetX = mouseX - lastMousePos.x;
        float offsetY = mouseY - lastMousePos.y;

        lastMousePos.x = mouseX;
        lastMousePos.y = mouseY;

        rotate(-offsetX * mouseSensitivity, -offsetY * mouseSensitivity);
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
