#pragma once
#include "Frustum.h"

#include "Core/Transform.h"

class Camera
{
	Transform transform;

	mutable glm::dvec3 forward;
	mutable glm::dvec3 up;
	mutable glm::dvec3 right;

	mutable Frustum frustum;

	mutable bool vectorsUpdateRequired = true;
	mutable bool frustumUpdateRequired = true;

	static glm::dvec3 worldUp;

	float FOV; // Radians
	float aspectRatio;
	float nearPlane, farPlane;

	void updateCameraVectors() const;
	void updateFrustum() const;
public:
	Camera(const glm::dvec3 position, float yaw, float pitch, float FOV, float aspectRatio, float nearPlane, float farPlane);

	glm::mat4 getViewMatrix() const;
	glm::mat4 getViewMatrixModified(const glm::dvec3& posMod) const;
	glm::mat4 getProjectionMatrix() const;

	void setPosition(const glm::dvec3& position);
	void setYaw(float yaw);
	void setPitch(float pitch);
	void setYawPitch(float yaw, float pitch);
	void setTransform(const Transform& transform);
	void setFOV(float fov);
	void setAspectRatio(float aspect);
	void setFarPlane(float farPlane);

	void move(const glm::dvec3& delta);
	void rotate(float deltaYaw, float deltaPitch);

	glm::dvec3 getPosition() const;
	float getYaw() const;
	float getPitch() const;
	Transform getTransform() const;
	glm::dvec3 getForward() const;
	glm::dvec3 getUp() const;
	glm::dvec3 getRight() const;
	const Frustum& getFrustum() const;
	float getNear() const;
	float getFar() const;
};

