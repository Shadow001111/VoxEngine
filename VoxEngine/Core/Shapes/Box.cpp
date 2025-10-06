#include "Box.h"

Box::Box() :
	center(0.0f), halfExtents(0.0f)
{
}

Box::Box(const glm::vec3& center, const glm::vec3& halfExtents) :
	center(center), halfExtents(halfExtents)
{
}
