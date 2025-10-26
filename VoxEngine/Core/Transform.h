#pragma once
#include <glm/glm.hpp>

// Contains position and rotation
struct Transform
{
	glm::dvec3 position;
	float yaw, pitch; // Radians

	Transform();
	Transform(const glm::dvec3& position, float yaw, float pitch);

	Transform interpolate(const Transform& other, double factor) const;
};

