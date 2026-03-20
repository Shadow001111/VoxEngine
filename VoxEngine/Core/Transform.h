#pragma once
#include <glm/glm.hpp>
#include "Core/CoreMath.h"

// Contains position and rotation
template<typename T>
struct Transform
{
	using Vec3Type = glm::vec<3, T>;

	Vec3Type position;
	T yaw, pitch; // Radians

	Transform();
	Transform(const Vec3Type& position, T yaw, T pitch);

	Transform interpolate(const Transform& other, T factor) const;
};

template<typename T>
inline Transform<T>::Transform() :
	position(0.0, 0.0, 0.0), yaw(0.0), pitch(0.0)
{
}

template<typename T>
inline Transform<T>::Transform(const Vec3Type& position, T yaw, T pitch) :
	position(position), yaw(yaw), pitch(pitch)
{
}

template<typename T>
inline Transform<T> Transform<T>::interpolate(const Transform& other, T factor) const
{
	return Transform<T>(
		lerp(position, other.position, factor),
		lerp(yaw, other.yaw, factor),
		lerp(pitch, other.pitch, factor)
	);
}
