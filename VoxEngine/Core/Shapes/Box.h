#pragma once
#include <glm/vec3.hpp>

struct Box
{
	glm::dvec3 center, halfExtents;

	Box();
	Box(const glm::dvec3& center, const glm::dvec3& halfExtents);
};

