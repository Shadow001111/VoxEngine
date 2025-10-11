#include "RGB.h"

RGB::RGB() :
	r(0.0f), g(0.0f), b(0.0f)
{
}

RGB::RGB(float value) :
	r(value), g(value), b(value)
{
}

RGB::RGB(float r, float g, float b) :
	r(r), g(g), b(b)
{
}
