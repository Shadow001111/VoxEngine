#include "Box.h"

Box::Box() :
	center(0.0), halfExtents(0.0)
{
}

Box::Box(const glm::dvec3& center, const glm::dvec3& halfExtents) :
	center(center), halfExtents(halfExtents)
{
}
