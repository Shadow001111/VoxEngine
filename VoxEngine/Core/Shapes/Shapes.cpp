#include "Shapes.h"

#include <glm/glm.hpp>

bool isBoxOnOrForwardPlane(const Box& box, const Plane& plane)
{
    auto distance = plane.distanceToPoint(box.center);
    auto effectiveRadius = glm::dot(box.halfExtents, glm::abs(plane.normal));
    return distance >= -effectiveRadius;
}

bool isBoxOnOrForwardPlane(const Box& box, const LitePlane& plane)
{
    auto distance = plane.distanceToPoint(box.center);
    auto effectiveRadius = glm::dot(box.halfExtents, glm::abs(plane.normal));
    return distance >= -effectiveRadius;
}
