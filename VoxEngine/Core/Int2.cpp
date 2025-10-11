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
	return x == other.x && y == other.y;
}

size_t Int2Hasher::operator()(const Int2& other) const
{
	constexpr size_t addConst = 0x9e3779b97f4a7c15;
	size_t h = (size_t)other.x + addConst;
	h ^=	   (size_t)other.y + addConst + (h << 6) + (h >> 2);
	return h;
}
