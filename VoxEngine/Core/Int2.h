#pragma once

struct Int2
{
	int x, y;

	Int2();
	Int2(int x, int y);

	bool operator==(const Int2& other) const;
};

struct Int2Hasher
{
public:
	size_t operator()(const Int2& other) const;
};