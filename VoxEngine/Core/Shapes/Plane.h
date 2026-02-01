#pragma once
#include <glm/glm.hpp>

struct Plane
{
	glm::dvec3 center, normal;

	Plane();
	Plane(const glm::dvec3& center, const glm::dvec3& normal);

	double distanceToPoint(const glm::dvec3& point) const { return glm::dot(normal, point - center); };
};

struct LitePlane
{
	glm::dvec3 normal;
	double d;

	LitePlane();
	LitePlane(const glm::dvec3& center, const glm::dvec3& normal);
	LitePlane(const glm::dvec3& normal, double d);

	double distanceToPoint(const glm::dvec3& point) const { return glm::dot(normal, point) + d; };
};

LitePlane toLitePlane(const Plane& plane);
Plane toPlane(const LitePlane& litePlane);
