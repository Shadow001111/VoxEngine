#include "BlockData.h"

#include "Core/StringIndexer.h"

#include <algorithm>
#include <cassert>

inline uint8_t clamp(uint8_t v, uint8_t min, uint8_t max)
{
	return std::min(max, std::max(min, v));
}


BlockProperties::BlockProperties(uint8_t lightAbsorption, uint8_t lightEmission, bool hasFaces, bool areFacesTransparent, bool raycastable) :
	lightAbsorption(clamp(lightAbsorption, 1, 15)),
	lightEmission(clamp(lightEmission, 0, 15)),
	hasFaces(hasFaces),
	areFacesTransparent(areFacesTransparent || !hasFaces),
	raycastable(raycastable)
{
}

BlockTextureNames::BlockTextureNames(const char* nxName, const char* pxName, const char* nyName, const char* pyName, const char* nzName, const char* pzName) :
	texName_negativeX(nxName), texName_positiveX(pxName),
	texName_negativeY(nyName), texName_positiveY(pyName),
	texName_negativeZ(nzName), texName_positiveZ(pzName)
{
}

BlockData::BlockData(const BlockProperties& properties, const BlockTextureNames& textureNames) :
	properties(properties), textureNames(textureNames)
{
}


BlockData BlockDataBase::BLOCK_DATABASE[(size_t)Block::__BlockCount__];

void BlockDataBase::registerBlock(Block block, const BlockProperties& properties, const BlockTextureNames& textureNames)
{
	BLOCK_DATABASE[(size_t)block] = { properties, textureNames};
}

// TODO: Import block data from some file. Store 'compiled' file in binary for fast loading. Check if file was updates by hashing.
void BlockDataBase::loadBlockDataBase()
{
	// TODO: Maybe rotate noise-like textures
	registerBlock(Block::Air, { 1,  0,  false, true, false }, { "", "", "", "", "", "" });
	registerBlock(Block::GrassBlock, { 15, 0,  true,  false, true}, {"grass_block_side", "grass_block_side", "dirt", "grass_block_top", "grass_block_side", "grass_block_side"});
	registerBlock(Block::Dirt, { 15, 0,  true,  false, true }, {"dirt", "dirt", "dirt", "dirt", "dirt", "dirt"});
	registerBlock(Block::Stone, { 15, 0,  true,  false, true }, {"stone", "stone", "stone", "stone", "stone", "stone"});
	registerBlock(Block::GlowStone, { 15, 15, true,  false, true }, {"glowstone", "glowstone", "glowstone", "glowstone", "glowstone", "glowstone"});
	registerBlock(Block::Glass, { 1, 0, true,  true, true }, { "glass", "glass", "glass", "glass", "glass", "glass" });
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

		if (!blockData->properties.hasFaces)
		{
			continue;
		}

		IDs.ids[0] = strInd.getID(blockData->textureNames.texName_negativeX);
		IDs.ids[1] = strInd.getID(blockData->textureNames.texName_positiveX);
		IDs.ids[2] = strInd.getID(blockData->textureNames.texName_negativeY);
		IDs.ids[3] = strInd.getID(blockData->textureNames.texName_positiveY);
		IDs.ids[4] = strInd.getID(blockData->textureNames.texName_negativeZ);
		IDs.ids[5] = strInd.getID(blockData->textureNames.texName_positiveZ);
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
