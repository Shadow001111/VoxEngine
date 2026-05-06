#pragma once
#include <glm/glm.hpp>

//template<typename T>
//struct Plane
//{
//	using value_type = T;
//
//	glm::vec<3, T> center, normal;
//
//	Plane() : center(T(0)), normal(T(0)) {}
//
//	Plane(const glm::vec<3, T>& center, const glm::vec<3, T>& normal)
//		: center(center), normal(normal) {
//	}
//
//	T distanceToPoint(const glm::vec<3, T>& point) const
//	{
//		return glm::dot(normal, point - center);
//	}
//};

template<typename T>
struct Plane
{
	using value_type = T;

	glm::vec<3, T> normal;
	T d;

	Plane() : normal(T(0)), d(T(0)) {}

	Plane(const glm::vec<3, T>& center, const glm::vec<3, T>& normal)
		: normal(normal), d(-glm::dot(normal, center)) {
	}

	Plane(const glm::vec<3, T>& normal, T d)
		: normal(normal), d(d) {
	}

	T distanceToPoint(const glm::vec<3, T>& point) const
	{
		return glm::dot(normal, point) + d;
	}
};

//template<typename T>
//LitePlane<T> toLitePlane(const Plane<T>& plane)
//{
//	return LitePlane<T>(plane.center, plane.normal);
//}
//
//template<typename T>
//Plane<T> toPlane(const LitePlane<T>& litePlane)
//{
//	const glm::vec<3, T>& normal = litePlane.normal;
//	T denom = glm::dot(normal, normal);
//	glm::vec<3, T> center = (denom != T(0)) ? (-litePlane.d / denom) * normal : glm::vec<3, T>(T(0));
//	return Plane<T>(center, normal);
//}