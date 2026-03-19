#pragma once
#include "Shapes.h"

template<typename PlaneType>
struct Frustum
{
	PlaneType top, bottom, right, left, far, near;

	using T = PlaneType::value_type;

	bool checkBox(const Box<T>& box) const
	{
		return
			isBoxOnOrForwardPlane(box, near) &&
			isBoxOnOrForwardPlane(box, far) &&
			isBoxOnOrForwardPlane(box, right) &&
			isBoxOnOrForwardPlane(box, left) &&
			isBoxOnOrForwardPlane(box, top) &&
			isBoxOnOrForwardPlane(box, bottom);
	}

	void update(const glm::vec<3, T>& position, const glm::vec<3, T>& forward,
		const glm::vec<3, T>& right, const glm::vec<3, T>& up,
		T fov, T aspectRatio, T nearPlane, T farPlane)
	{
		T halfVSide = farPlane * glm::tan(fov * T(0.5));
		T halfHSide = halfVSide * aspectRatio;

		glm::vec<3, T> nearMultFwd = nearPlane * forward;
		glm::vec<3, T> farMultFwd = farPlane * forward;

		near = PlaneType(position + nearMultFwd, forward);
		far = PlaneType(position + farMultFwd, -forward);

		this->right = PlaneType(position, glm::cross(farMultFwd - right * halfHSide, up));
		left = PlaneType(position, glm::cross(up, farMultFwd + right * halfHSide));

		top = PlaneType(position, glm::cross(right, farMultFwd - up * halfVSide));
		bottom = PlaneType(position, glm::cross(farMultFwd + up * halfVSide, right));
	}
};