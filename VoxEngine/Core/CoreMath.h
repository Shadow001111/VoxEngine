#pragma once

template<typename T, typename T2>
static inline T lerp(const T& a, const T& b, T2 factor)
{
	return a + (b - a) * factor;
}