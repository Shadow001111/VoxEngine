#pragma once
#include "Block.h"

struct BlockData
{
	const char* texName_negativeX = nullptr;
	const char* texName_positiveX = nullptr;
	const char* texName_negativeY = nullptr;
	const char* texName_positiveY = nullptr;
	const char* texName_negativeZ = nullptr;
	const char* texName_positiveZ = nullptr;

	BlockData(const char* nxName, const char* pxName, const char* nyName, const char* pyName, const char* nzName, const char* pzName);
};

class BlockDataBase
{
	static BlockData BLOCK_DATABASE[];
public:
	static inline const BlockData& getBlockData(Block block);
};