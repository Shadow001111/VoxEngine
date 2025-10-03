#pragma once

template<typename T>
static inline T lerp(const T& a, const T& b, float factor)
{
	return a + (b - a) * factor;
}