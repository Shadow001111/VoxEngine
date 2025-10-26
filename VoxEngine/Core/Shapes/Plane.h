#pragma once
#include <glm/vec3.hpp>

struct Plane
{
	glm::dvec3 center, normal;

	Plane();
	Plane(const glm::dvec3& center, const glm::dvec3& normal);

	float distanceToPoint(const glm::dvec3& point) const;
};

