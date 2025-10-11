#include "Int3.h"

#include <xhash>
#include <cassert>

Int3::Int3() :
	x(0), y(0), z(0)
{
}

Int3::Int3(int x, int y, int z) :
	x(x), y(y), z(z)
{
}

bool Int3::operator==(const Int3& other) const
{
	return x == other.x && y == other.y && z == other.z;
}

size_t Int3Hasher::operator()(const Int3& other) const
{
	constexpr size_t addConst = 0x9e3779b97f4a7c15;
	size_t h = (size_t)other.x + addConst;
	h ^=	   (size_t)other.y + addConst + (h << 6) + (h >> 2);
	h ^=	   (size_t)other.z + addConst + (h << 6) + (h >> 2);
	return h;
}
