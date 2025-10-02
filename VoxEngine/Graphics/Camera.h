#pragma once
#include <glm/glm.hpp>

class Camera
{
public:
	glm::vec3 position;

	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;

	static glm::vec3 worldUp;

	float yaw, pitch; // Radians

	float FOV; // Radians
	float aspectRatio;
	float nearPlane, farPlane;

	Camera(const glm::vec3 position, float yaw, float pitch, float FOV, float aspectRatio, float nearPlane, float farPlane);

	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix() const;
	void setAspectRatio(float aspect);
	void setFOV(float fov);
	void setYawPitch(float yaw, float pitch);
private:
	void updateCameraVectors();
};

