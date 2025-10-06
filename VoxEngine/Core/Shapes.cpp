#include "Shapes.h"

#include <glm/glm.hpp>

bool isBoxOnOrForwardPlane(const Box& box, const Plane& plane)
{
    return plane.distanceToPoint(box.center) >= -glm::dot(box.extents, glm::abs(plane.normal));
}
