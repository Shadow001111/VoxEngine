#pragma once
#include <glm/vec3.hpp>

struct Box
{
	glm::vec3 center, extents;

	Box();
	Box(const glm::vec3& center, const glm::vec3& extents);
};

