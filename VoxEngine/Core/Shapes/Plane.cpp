#include "Plane.h"
#include <glm/glm.hpp>

Plane::Plane() :
	center(0.0f), normal(0.0f)
{
}

Plane::Plane(const glm::vec3& center, const glm::vec3& normal) :
	center(center), normal(normal)
{
}

float Plane::distanceToPoint(const glm::vec3& point) const
{
	return glm::dot(normal, point - center);
}
