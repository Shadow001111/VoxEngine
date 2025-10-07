#pragma once
#include "Block.h"

struct BlockData
{
	// Texture names for each face: Top, Bottom, North, South, East, West
	const char* texName_negativeX = nullptr;
	const char* texName_positiveX = nullptr;
	const char* texName_negativeY = nullptr;
	const char* texName_positiveY = nullptr;
	const char* texName_negativeZ = nullptr;
	const char* texName_positiveZ = nullptr;

	uint16_t texID_negativeX = 0;
	uint16_t texID_positiveX = 0;
	uint16_t texID_negativeY = 0;
	uint16_t texID_positiveY = 0;
	uint16_t texID_negativeZ = 0;
	uint16_t texID_positiveZ = 0;

	BlockData(const char* nxName, const char* pxName, const char* nyName, const char* pyName, const char* nzName, const char* pzName);
};

class BlockDataBase
{
	static BlockData BLOCK_DATABASE[];
public:
	static inline const BlockData& getBlockData(Block block);
};