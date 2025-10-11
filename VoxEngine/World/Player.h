#pragma once
#include "WindowManager.h" // TODO: Should be removed, but needed for input
#include "Graphics/Camera.h"

class Player
{
	Transform transform;
	Transform previousTransform;

	Camera camera;
public:
	Player(const glm::vec3& position, float yaw, float pitch);

	void update(const WindowManager& wnd, float deltaTime, glm::vec2& lastMousePos);
	void interpolateCameraTransform(float factor);

	void setPosition(const glm::vec3& position);
	void setYaw(float yaw);
	void setPitch(float pitch);
	void setYawPitch(float yaw, float pitch);
	void setTransform(const Transform& transform);

	void move(const glm::vec3& delta);
	void rotate(float deltaYaw, float deltaPitch);

	glm::vec3 getPosition() const;
	float getYaw() const;
	float getPitch() const;
	Transform getTransform() const;
	Transform getPreviousTransform() const;
	Camera& getCamera();
};

