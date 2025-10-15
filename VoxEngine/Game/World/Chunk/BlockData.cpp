#include "BlockData.h"

#include "Core/StringIndexer.h"

#include <algorithm>
#include <cassert>

inline uint8_t clamp(uint8_t v, uint8_t min, uint8_t max)
{
	return std::min(max, std::max(min, v));
}

BlockData::BlockData(
	uint8_t lightAbsorption,
	uint8_t lightEmission,
	bool hasTransparentFaces,
	const char* nxName, const char* pxName,
	const char* nyName, const char* pyName,
	const char* nzName, const char* pzName)
	:
	lightAbsorption(clamp(lightAbsorption, 1, 15)),
	lightEmission(clamp(lightEmission, 0, 15)),
	hasTransparentFaces(hasTransparentFaces),
	texName_negativeX(nxName), texName_positiveX(pxName),
	texName_negativeY(nyName), texName_positiveY(pyName),
	texName_negativeZ(nzName), texName_positiveZ(pzName)
{
	
}


BlockData BlockDataBase::BLOCK_DATABASE[(size_t)Block::__BlockCount__];

void BlockDataBase::registerBlock(Block block, const BlockData& blockData)
{
	BLOCK_DATABASE[(size_t)block] = blockData;
}

void BlockDataBase::loadBlockDataBase()
{
	registerBlock(Block::Air,        { 1,  0,  true,  "", "", "", "", "", "" });
	registerBlock(Block::GrassBlock, { 15, 0,  false, "grass_block_side", "grass_block_side", "dirt", "grass_block_top", "grass_block_side", "grass_block_side" });
	registerBlock(Block::Dirt,       { 15, 0,  false, "dirt", "dirt", "dirt", "dirt", "dirt", "dirt" });
	registerBlock(Block::Stone,      { 15, 0,  false, "stone", "stone", "stone", "stone", "stone", "stone" });
	registerBlock(Block::GlowStone,  { 15, 15, false, "glowstone", "glowstone", "glowstone", "glowstone", "glowstone", "glowstone" });
}

const BlockData* BlockDataBase::getBlockData(Block block)
{
	return &BLOCK_DATABASE[static_cast<size_t>(block)];
}

const BlockData* BlockDataBase::getBlockData(size_t index)
{
	if (index >= (size_t)Block::__BlockCount__)
	{
		return &BLOCK_DATABASE[0];
	}
	return &BLOCK_DATABASE[index];
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
		const BlockData* blockData = BlockDataBase::getBlockData(blockID);

		IDs.ids[0] = strInd.getID(blockData->texName_negativeX);
		IDs.ids[1] = strInd.getID(blockData->texName_positiveX);
		IDs.ids[2] = strInd.getID(blockData->texName_negativeY);
		IDs.ids[3] = strInd.getID(blockData->texName_positiveY);
		IDs.ids[4] = strInd.getID(blockData->texName_negativeZ);
		IDs.ids[5] = strInd.getID(blockData->texName_positiveZ);
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
