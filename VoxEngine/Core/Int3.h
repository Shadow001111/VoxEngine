#pragma once

struct Int3
{
	int x, y, z;

	Int3();
	Int3(int x, int y, int z);

	bool operator==(const Int3& other) const;
};