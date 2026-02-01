#pragma once
#include "Shapes.h"

struct Frustum
{
	Plane top;
	Plane bottom;
	Plane right;
	Plane left;
	Plane far;
	Plane near;

	bool checkBox(const Box& box) const;
};

struct LiteFrustum
{
	LitePlane top;
	LitePlane bottom;
	LitePlane right;
	LitePlane left;
	LitePlane far;
	LitePlane near;

	bool checkBox(const Box& box) const;
};

