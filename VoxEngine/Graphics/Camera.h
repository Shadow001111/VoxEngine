#pragma once
#include "Transform.h"

class Camera
{
	Transform transform;

	mutable glm::vec3 front;
	mutable glm::vec3 up;
	mutable glm::vec3 right;
	mutable bool vectorsUpdateRequired = true;

	static glm::vec3 worldUp;

	float FOV; // Radians
	float aspectRatio;
	float nearPlane, farPlane;
public:
	Camera(const glm::vec3 position, float yaw, float pitch, float FOV, float aspectRatio, float nearPlane, float farPlane);

	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix() const;

	void setPosition(const glm::vec3& position);
	void setYaw(float yaw);
	void setPitch(float pitch);
	void setYawPitch(float yaw, float pitch);
	void setTransform(const Transform& transform);
	void setAspectRatio(float aspect);
	void setFOV(float fov);

	void move(const glm::vec3& delta);
	void rotate(float deltaYaw, float deltaPitch);

	glm::vec3 getPosition() const;
	float getYaw() const;
	float getPitch() const;
	Transform getTransform() const;
	glm::vec3 getFront() const;
	glm::vec3 getUp() const;
	glm::vec3 getRight() const;

	void updateCameraVectors() const;
};

