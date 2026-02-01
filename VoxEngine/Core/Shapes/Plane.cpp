#include "Plane.h"

Plane::Plane() :
	center(0.0), normal(0.0)
{
}

Plane::Plane(const glm::dvec3& center, const glm::dvec3& normal) :
	center(center), normal(normal)
{
}


LitePlane::LitePlane() :
	normal(0.0), d(0.0)
{
}

LitePlane::LitePlane(const glm::dvec3& center, const glm::dvec3& normal) :
	normal(normal), d(-glm::dot(normal, center))
{
}

LitePlane::LitePlane(const glm::dvec3& normal, double d) :
	normal(normal), d(d)
{
}

LitePlane toLitePlane(const Plane& plane)
{
	return LitePlane(plane.center, plane.normal);
}

Plane toPlane(const LitePlane& litePlane)
{
	const glm::dvec3& normal = litePlane.normal;

	double denom = glm::dot(normal, normal);
	glm::dvec3 center = (denom != 0.0) ? (-litePlane.d / denom) * normal : glm::dvec3(0.0);

	return Plane(center, normal);
}
