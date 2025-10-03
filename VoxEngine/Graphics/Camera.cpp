#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

glm::vec3 Camera::worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

Camera::Camera(const glm::vec3 position, float yaw, float pitch, float FOV, float aspectRatio, float nearPlane, float farPlane) :
	transform(position, yaw, pitch), FOV(FOV), aspectRatio(aspectRatio), nearPlane(nearPlane), farPlane(farPlane)
{
	updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
	updateCameraVectors();
	return glm::lookAt(transform.position, transform.position + front, up);
}

glm::mat4 Camera::getProjectionMatrix() const
{
	return glm::perspective(FOV, aspectRatio, nearPlane, farPlane);
}

void Camera::setPosition(const glm::vec3& position)
{
	transform.position = position;
}

void Camera::setYaw(float yaw)
{
	transform.yaw = yaw;
	vectorsUpdateRequired = true;
}

void Camera::setPitch(float pitch)
{
	transform.pitch = glm::clamp(pitch, -1.5707f, 1.5707f);
	vectorsUpdateRequired = true;
}

void Camera::setYawPitch(float yaw, float pitch)
{
	transform.yaw = yaw;
	transform.pitch = glm::clamp(pitch, -1.5707f, 1.5707f);
	vectorsUpdateRequired = true;
}

void Camera::setTransform(const Transform& transform)
{
	this->transform = transform;
	this->transform.pitch = glm::clamp(this->transform.pitch, -1.5707f, 1.5707f);
	vectorsUpdateRequired = true;
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

void Camera::move(const glm::vec3& delta)
{
	transform.position += delta;
}

void Camera::rotate(float deltaYaw, float deltaPitch)
{
	transform.yaw += deltaYaw;
	transform.pitch += deltaPitch;
	transform.pitch = glm::clamp(transform.pitch, -1.5707f, 1.5707f);
	vectorsUpdateRequired = true;
}

glm::vec3 Camera::getPosition() const
{
	return transform.position;
}

float Camera::getYaw() const
{
	return transform.yaw;
}

float Camera::getPitch() const
{
	return transform.pitch;
}

Transform Camera::getTransform() const
{
	return transform;
}

glm::vec3 Camera::getFront() const
{
	updateCameraVectors();
	return front;
}

glm::vec3 Camera::getUp() const
{
	updateCameraVectors();
	return up;
}

glm::vec3 Camera::getRight() const
{
	updateCameraVectors();
	return right;
}

void Camera::updateCameraVectors() const
{
	if (!vectorsUpdateRequired) return;
	vectorsUpdateRequired = false;

	// Calculate the new Front vector
	glm::vec3 front;
	front.x = sin(transform.yaw) * cos(transform.pitch);
	front.y = sin(transform.pitch);
	front.z = cos(transform.yaw) * cos(transform.pitch);
	this->front = glm::normalize(front);

	// Calculate the Right and Up vectors
	right = glm::normalize(glm::cross(this->front, worldUp));
	up = glm::normalize(glm::cross(right, this->front));
}
