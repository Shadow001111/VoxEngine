#pragma once
#include <vector>
#include <cstdint>

class ChunkNormalPacker
{
	std::vector<uint32_t> data;
	uint32_t normalsPackedInCurrentRegion = 0;
public:
	void addNormal(uint32_t value);
	void clear();

	size_t getSize() const { return data.size(); };
	const uint32_t* getData() const { return data.data(); };
};

