#pragma once
#include "Shapes.h"

struct Frustum
{
	Plane topPlane;
	Plane bottomPlane;
	Plane rightPlane;
	Plane leftPlane;
	Plane farPlane;
	Plane nearPlane;

	void update(
		const glm::dvec3& position, const glm::dvec3& forward,
		const glm::dvec3& right, const glm::dvec3& up,
		double fov, double aspectRatio, double nearPlane, double farPlane
	);

	bool checkBox(const Box& box) const;
};

struct LiteFrustum
{
	LitePlane topPlane;
	LitePlane bottomPlane;
	LitePlane rightPlane;
	LitePlane leftPlane;
	LitePlane farPlane;
	LitePlane nearPlane;

	void update(
		const glm::dvec3& position, const glm::dvec3& forward,
	    const glm::dvec3& right, const glm::dvec3& up,
	    double fov, double aspectRatio, double nearPlane, double farPlane
	);

	bool checkBox(const Box& box) const;
};

