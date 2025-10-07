#include "BlockData.h"

BlockData::BlockData(const char* nxName, const char* pxName, const char* nyName, const char* pyName, const char* nzName, const char* pzName) :
	texName_negativeX(nxName), texName_positiveX(pxName),
	texName_negativeY(nyName), texName_positiveY(pyName),
	texName_negativeZ(nzName), texName_positiveZ(pzName)
{
}


BlockData BlockDataBase::BLOCK_DATABASE[] =
{
	// Air 0
	{"", "", "", "", "", ""},

	// GrassBlock 1
	{"grass_block_side", "grass_block_side", "dirt", "grass_block_top", "grass_block_side", "grass_block_side"}
};

inline const BlockData& BlockDataBase::getBlockData(Block block)
{
	return BLOCK_DATABASE[static_cast<size_t>(block)];
}
