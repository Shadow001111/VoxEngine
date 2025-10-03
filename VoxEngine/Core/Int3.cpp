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
