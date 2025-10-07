#include "BlockData.h"

#include "StringIndexer.h"

#include <algorithm>

BlockData::BlockData(const char* nxName, const char* pxName, const char* nyName, const char* pyName, const char* nzName, const char* pzName) :
	texName_negativeX(nxName), texName_positiveX(pxName),
	texName_negativeY(nyName), texName_positiveY(pyName),
	texName_negativeZ(nzName), texName_positiveZ(pzName)
{
}


BlockData BlockDataBase::BLOCK_DATABASE[(size_t)Block::__BlockCount__] =
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

inline const BlockData& BlockDataBase::getBlockData(size_t index)
{
	// TODO: Maybe add check condition?
	return BLOCK_DATABASE[index];
}


BlockTextureIDDatabase::BlockTextureIDDatabase()
{
	// Create array
	const size_t blockCount = (size_t)Block::__BlockCount__;
	blockTexturesIDs = new BlockTextureIDs[blockCount];
}

BlockTextureIDDatabase::~BlockTextureIDDatabase()
{
	delete[] blockTexturesIDs;
}

void BlockTextureIDDatabase::build(std::vector<std::string>& textureNames)
{
	// Assign texture IDs
	StringIndexer strInd;

	const size_t blockCount = (size_t)Block::__BlockCount__;
	for (size_t blockID = 0; blockID < blockCount; blockID++)
	{
		BlockTextureIDs& IDs = blockTexturesIDs[blockID];
		const BlockData& blockData = BlockDataBase::getBlockData(blockID);

		IDs.ids[0] = strInd.getID(blockData.texName_negativeX);
		IDs.ids[1] = strInd.getID(blockData.texName_positiveX);
		IDs.ids[2] = strInd.getID(blockData.texName_negativeY);
		IDs.ids[3] = strInd.getID(blockData.texName_positiveY);
		IDs.ids[4] = strInd.getID(blockData.texName_negativeZ);
		IDs.ids[5] = strInd.getID(blockData.texName_positiveZ);
	}

	// Collect names
	textureNames.clear();
	const auto& nameToIDMap = strInd.getNameToIDMap();
	for (const auto& pair : nameToIDMap)
	{
		textureNames.push_back(pair.first);
	}

	// Sort names by index
	std::sort(textureNames.begin(), textureNames.end(), [&nameToIDMap](const std::string& a, const std::string& b)
	{
		return nameToIDMap.find(a)->second < nameToIDMap.find(b)->second; // ascending order
	});
}

const BlockTextureIDDatabase::BlockTextureIDs& BlockTextureIDDatabase::getBlockTextureIDs(Block block) const
{
	return blockTexturesIDs[static_cast<size_t>(block)];
}
