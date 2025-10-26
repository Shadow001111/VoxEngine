#include "Plane.h"
#include <glm/glm.hpp>

Plane::Plane() :
	center(0.0), normal(0.0)
{
}

Plane::Plane(const glm::dvec3& center, const glm::dvec3& normal) :
	center(center), normal(normal)
{
}

float Plane::distanceToPoint(const glm::dvec3& point) const
{
	return glm::dot(normal, point - center);
}
