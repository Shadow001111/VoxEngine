#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

glm::vec3 Camera::worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

Camera::Camera(const glm::vec3 position, float yaw, float pitch, float FOV, float aspectRatio, float nearPlane, float farPlane) :
	position(position), yaw(yaw), pitch(pitch), FOV(FOV), aspectRatio(aspectRatio), nearPlane(nearPlane), farPlane(farPlane)
{
	updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
	return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix() const
{
	return glm::perspective(FOV, aspectRatio, nearPlane, farPlane);
}

void Camera::setAspectRatio(float aspect)
{
	aspectRatio = aspect;
}

void Camera::setFOV(float fov)
{
	if (fov < 1.0f) fov = 1.0f;
	if (fov > 90.0f) fov = 90.0f;
	FOV = fov;
}

void Camera::setYawPitch(float yaw, float pitch)
{
	this->yaw = yaw;
	this->pitch = glm::clamp(pitch, -1.5707f, 1.5707f);
	updateCameraVectors();
}

void Camera::updateCameraVectors()
{
	// Calculate the new Front vector
	glm::vec3 front;
	front.x = sin(yaw) * cos(pitch);
	front.y = sin(pitch);
	front.z = cos(yaw) * cos(pitch);
	this->front = glm::normalize(front);

	// Calculate the Right and Up vectors
	right = glm::normalize(glm::cross(this->front, worldUp));
	up = glm::normalize(glm::cross(right, this->front));
}
