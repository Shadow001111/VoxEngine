#include "ChunkNormalPacker.h"

void ChunkNormalPacker::addNormal(uint32_t value)
{
    // Ensure we're only using 3 bits
    //value &= 0x7;  // Mask to 3 bits (0b111)

    // If we need a new region
    if (data.empty() || normalsPackedInCurrentRegion >= 10)
    {
        data.push_back(0);
        normalsPackedInCurrentRegion = 0;
    }

    // Calculate the bit position: 3 bits per normal
    uint32_t bit_pos = normalsPackedInCurrentRegion * 3;

    // Pack the value into the current region
    data.back() |= (value << bit_pos);

    // Increment the count
    normalsPackedInCurrentRegion++;
}

void ChunkNormalPacker::clear()
{
	data.clear();
    normalsPackedInCurrentRegion = 0;
}
