#pragma once
#include <glm/glm.hpp>

// Contains position and rotation
struct Transform
{
	glm::vec3 position;
	float yaw, pitch; // Radians

	Transform();
	Transform(const glm::vec3& position, float yaw, float pitch);

	Transform interpolate(const Transform& other, float factor) const;
};

