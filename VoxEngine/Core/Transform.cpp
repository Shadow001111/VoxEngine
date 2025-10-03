#include "Transform.h"
#include "CoreMath.h"


Transform::Transform() :
	position(0.0f, 0.0f, 0.0f), yaw(0.0f), pitch(0.0f)
{
}

Transform::Transform(const glm::vec3& position, float yaw, float pitch) :
	position(position), yaw(yaw), pitch(pitch)
{
}

Transform Transform::interpolate(const Transform& other, float factor) const
{
	return Transform(
		lerp(position, other.position, factor),
		lerp(yaw, other.yaw, factor),
		lerp(pitch, other.pitch, factor)
	);
}
