#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

glm::dvec3 Camera::worldUp = glm::vec3(0.0, 1.0, 0.0);

void Camera::updateCameraVectors() const
{
	if (!vectorsUpdateRequired) return;
	vectorsUpdateRequired = false;

	// Calculate the new Front vector
	glm::dvec3 front;
	front.x = sin(transform.yaw) * cos(transform.pitch);
	front.y = sin(transform.pitch);
	front.z = cos(transform.yaw) * cos(transform.pitch);
	this->forward = glm::normalize(front);

	// Calculate the Right and Up vectors
	right = glm::normalize(glm::cross(this->forward, worldUp));
	up = glm::normalize(glm::cross(right, this->forward));
}

void Camera::updateFrustum() const
{
	if (!frustumUpdateRequired) return;
	frustumUpdateRequired = false;

	updateCameraVectors();

	//
	double tanHF = tan((double)FOV * 0.5);
	double tanHFAR = tanHF * (double)aspectRatio;

	double halfVSide = farPlane * tanHF;
	double halfHSide = halfVSide * aspectRatio;

	glm::dvec3 nearMultFar = (double)nearPlane * forward;
	glm::dvec3 frontMultFar = (double)farPlane * forward;

	frustum.near = Plane(transform.position + nearMultFar, forward);
	frustum.far = Plane(transform.position + frontMultFar, -forward);

	frustum.right = Plane(transform.position, glm::cross(frontMultFar - right * halfHSide, up));
	frustum.left = Plane(transform.position, glm::cross(up, frontMultFar + right * halfHSide));

	frustum.top = Plane(transform.position, glm::cross(right, frontMultFar - up * halfVSide));
	frustum.bottom = Plane(transform.position, glm::cross(frontMultFar + up * halfVSide, right));
}

Camera::Camera(const glm::dvec3 position, float yaw, float pitch, float FOV, float aspectRatio, float nearPlane, float farPlane) :
	transform(position, yaw, pitch), FOV(FOV), aspectRatio(aspectRatio), nearPlane(nearPlane), farPlane(farPlane)
{
}

glm::mat4 Camera::getViewMatrix() const
{
	updateCameraVectors();
	return glm::lookAt(transform.position, transform.position + forward, up);
}

glm::mat4 Camera::getProjectionMatrix() const
{
	return glm::perspective(FOV, aspectRatio, nearPlane, farPlane);
}

void Camera::setPosition(const glm::dvec3& position)
{
	transform.position = position;
	frustumUpdateRequired = true;
}

void Camera::setYaw(float yaw)
{
	transform.yaw = yaw;
	vectorsUpdateRequired = true;
	frustumUpdateRequired = true;
}

void Camera::setPitch(float pitch)
{
	transform.pitch = glm::clamp(pitch, -1.5707f, 1.5707f);
	vectorsUpdateRequired = true;
	frustumUpdateRequired = true;
}

void Camera::setYawPitch(float yaw, float pitch)
{
	transform.yaw = yaw;
	transform.pitch = glm::clamp(pitch, -1.5707f, 1.5707f);
	vectorsUpdateRequired = true;
	frustumUpdateRequired = true;
}

void Camera::setTransform(const Transform& transform)
{
	this->transform = transform;
	this->transform.pitch = glm::clamp(this->transform.pitch, -1.5707f, 1.5707f);
	vectorsUpdateRequired = true;
	frustumUpdateRequired = true;
}

void Camera::setFOV(float fov)
{
	if (fov < 1.0f) fov = 1.0f;
	if (fov > 90.0f) fov = 90.0f;
	FOV = fov;
	frustumUpdateRequired = true;
}

void Camera::setAspectRatio(float aspect)
{
	aspectRatio = aspect;
	frustumUpdateRequired = true;
}

void Camera::setFarPlane(float farPlane)
{
	this->farPlane = farPlane;
	frustumUpdateRequired = true;
}

void Camera::move(const glm::dvec3& delta)
{
	transform.position += delta;
	frustumUpdateRequired = true;
}

void Camera::rotate(float deltaYaw, float deltaPitch)
{
	transform.yaw += deltaYaw;
	transform.pitch += deltaPitch;
	transform.pitch = glm::clamp(transform.pitch, -1.5707f, 1.5707f);
	vectorsUpdateRequired = true;
	frustumUpdateRequired = true;
}

glm::dvec3 Camera::getPosition() const
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

glm::dvec3 Camera::getForward() const
{
	updateCameraVectors();
	return forward;
}

glm::dvec3 Camera::getUp() const
{
	updateCameraVectors();
	return up;
}

glm::dvec3 Camera::getRight() const
{
	updateCameraVectors();
	return right;
}

const Frustum& Camera::getFrustum() const
{
	updateFrustum();
	return frustum;
}

float Camera::getNear() const
{
	return nearPlane;
}

float Camera::getFar() const
{
	return farPlane;
}
