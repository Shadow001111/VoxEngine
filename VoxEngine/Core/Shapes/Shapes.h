#pragma once
#include "Box.h"
#include "Plane.h"

bool isBoxOnOrForwardPlane(const Box& box, const Plane& plane);
bool isBoxOnOrForwardPlane(const Box& box, const LitePlane& plane);