#include "Int2.h"

Int2::Int2() :
	x(0), y(0)
{
}

Int2::Int2(int x, int y) :
	x(x), y(y)
{
}

bool Int2::operator==(const Int2& other) const
{
	return false;
}

size_t Int2Hasher::operator()(const Int2& other) const
{
	size_t x = (size_t)other.x;
	size_t y = (size_t)other.y;
	return x | (y << 32);
}
