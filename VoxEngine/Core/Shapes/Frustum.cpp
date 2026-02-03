#include "Frustum.h"

bool Frustum::checkBox(const Box& box) const
{
	return isBoxOnOrForwardPlane(box, near) &&
		   isBoxOnOrForwardPlane(box, far) &&
		   isBoxOnOrForwardPlane(box, right) &&
		   isBoxOnOrForwardPlane(box, left) &&
		   isBoxOnOrForwardPlane(box, top) &&
		   isBoxOnOrForwardPlane(box, bottom);
}

bool LiteFrustum::checkBox(const Box& box) const
{
	return isBoxOnOrForwardPlane(box, near) &&
		   isBoxOnOrForwardPlane(box, far) &&
		   isBoxOnOrForwardPlane(box, right) &&
		   isBoxOnOrForwardPlane(box, left) &&
		   isBoxOnOrForwardPlane(box, top) &&
		   isBoxOnOrForwardPlane(box, bottom);
}

bool LighterFrustum::checkBox(const Box& box) const
{
	return isBoxOnOrForwardPlane(box, far) &&
		   isBoxOnOrForwardPlane(box, right) &&
		   isBoxOnOrForwardPlane(box, left) &&
		   isBoxOnOrForwardPlane(box, top) &&
		   isBoxOnOrForwardPlane(box, bottom);
}
