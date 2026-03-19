#include "Frustum.h"

void Frustum::update(
	const glm::dvec3& position,
	const glm::dvec3& forward,
	const glm::dvec3& right,
	const glm::dvec3& up,
	double fov,
	double aspectRatio,
	double nearPlane,
	double farPlane
)
{
	double tanHF = tan(fov * 0.5);
	double halfVSide = farPlane * tanHF;
	double halfHSide = halfVSide * aspectRatio;

	glm::dvec3 nearMultFwd = nearPlane * forward;
	glm::dvec3 farMultFwd = farPlane * forward;

	this->nearPlane = Plane(position + nearMultFwd, forward);
	this->farPlane = Plane(position + farMultFwd, -forward);

	rightPlane = Plane(position, glm::cross(farMultFwd - right * halfHSide, up));
	leftPlane = Plane(position, glm::cross(up, farMultFwd + right * halfHSide));

	topPlane = Plane(position, glm::cross(right, farMultFwd - up * halfVSide));
	bottomPlane = Plane(position, glm::cross(farMultFwd + up * halfVSide, right));
}

bool Frustum::checkBox(const Box& box) const
{
	return
		isBoxOnOrForwardPlane(box, nearPlane) &&
		isBoxOnOrForwardPlane(box, farPlane) &&
		isBoxOnOrForwardPlane(box, rightPlane) &&
		isBoxOnOrForwardPlane(box, leftPlane) &&
		isBoxOnOrForwardPlane(box, topPlane) &&
		isBoxOnOrForwardPlane(box, bottomPlane);
}

void LiteFrustum::update(
	const glm::dvec3& position,
	const glm::dvec3& forward,
	const glm::dvec3& right,
	const glm::dvec3& up,
	double fov,
	double aspectRatio,
	double nearPlane,
	double farPlane
)
{
	double tanHF = tan(fov * 0.5);
	double halfVSide = farPlane * tanHF;
	double halfHSide = halfVSide * aspectRatio;

	glm::dvec3 nearMultFwd = nearPlane * forward;
	glm::dvec3 farMultFwd = farPlane * forward;

	this->nearPlane = LitePlane(position + nearMultFwd, forward);
	this->farPlane = LitePlane(position + farMultFwd, -forward);

	rightPlane = LitePlane(position, glm::cross(farMultFwd - right * halfHSide, up));
	leftPlane = LitePlane(position, glm::cross(up, farMultFwd + right * halfHSide));

	topPlane = LitePlane(position, glm::cross(right, farMultFwd - up * halfVSide));
	bottomPlane = LitePlane(position, glm::cross(farMultFwd + up * halfVSide, right));
}

bool LiteFrustum::checkBox(const Box& box) const
{
	return
		isBoxOnOrForwardPlane(box, nearPlane) &&
		isBoxOnOrForwardPlane(box, farPlane) &&
		isBoxOnOrForwardPlane(box, rightPlane) &&
		isBoxOnOrForwardPlane(box, leftPlane) &&
		isBoxOnOrForwardPlane(box, topPlane) &&
		isBoxOnOrForwardPlane(box, bottomPlane);
}
