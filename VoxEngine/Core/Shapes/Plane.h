#pragma once
#include <glm/vec3.hpp>

struct Plane
{
	glm::vec3 center, normal;

	Plane();
	Plane(const glm::vec3& center, const glm::vec3& normal);

	float distanceToPoint(const glm::vec3& point) const;
};

