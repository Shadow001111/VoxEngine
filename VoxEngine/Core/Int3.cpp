#include "Int3.h"

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
	constexpr size_t mask = (size_t)0x1FFFFF;
	size_t x = ((size_t)other.x) & mask;
	size_t y = ((size_t)other.y) & mask;
	size_t z = ((size_t)other.z) & mask;
	return x | (y << 21) | (z << 42);
}
