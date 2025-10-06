#include "Box.h"

Box::Box() :
	center(0.0f), extents(0.0f)
{
}

Box::Box(const glm::vec3& center, const glm::vec3& extents) :
	center(center), extents(extents)
{
}
