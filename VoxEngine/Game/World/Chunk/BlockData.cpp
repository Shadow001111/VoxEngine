#include "BlockData.h"

#include "Core/StringIndexer.h"

#include <algorithm>
#include <cassert>
#include <iostream>

inline uint8_t clamp(uint8_t v, uint8_t min, uint8_t max)
{
	return std::min(max, std::max(min, v));
}


BlockProperties::BlockProperties(bool absorbsLight, uint8_t lightEmission, bool hasFaces, bool areFacesTransparent, bool raycastable) :
	absorbsLight(absorbsLight),
	lightEmission(clamp(lightEmission, 0, 15)),
	hasFaces(hasFaces),
	areFacesTransparent(areFacesTransparent || !hasFaces),
	raycastable(raycastable)
{
}

BlockTextures::BlockTextures(
	const char* nxName, const char* pxName,
	const char* nyName, const char* pyName,
	const char* nzName, const char* pzName,
	TextureTransformation nxTransform, TextureTransformation pxTransform,
	TextureTransformation nyTransform, TextureTransformation pyTransform,
	TextureTransformation nzTransform, TextureTransformation pzTransform)
	:
	texName_negativeX(nxName), texName_positiveX(pxName),
	texName_negativeY(nyName), texName_positiveY(pyName),
	texName_negativeZ(nzName), texName_positiveZ(pzName)
{
	texturesTransformation =  uint16_t(nxTransform);
	texturesTransformation |= uint16_t(pxTransform) << 2u;
	texturesTransformation |= uint16_t(nyTransform) << 4u;
	texturesTransformation |= uint16_t(pyTransform) << 6u;
	texturesTransformation |= uint16_t(nzTransform) << 8u;
	texturesTransformation |= uint16_t(pzTransform) << 10u;
}

BlockData::BlockData(const BlockProperties& properties, const BlockTextures& textures) :
	properties(properties), textures(textures)
{
}


BlockData BlockDataBase::BLOCK_DATABASE[(size_t)Block::__BlockCount__];

void BlockDataBase::registerBlock(Block block, const BlockProperties& properties, const BlockTextures& textures)
{
	BLOCK_DATABASE[(size_t)block] = { properties, textures};
}

// TODO: Import block data from some file. Store 'compiled' file in binary for fast loading. Check if file was updates by hashing.
// TODO: Remove big and hard constructors
void BlockDataBase::loadBlockDataBase()
{
	registerBlock(Block::Air,
		{ false,  0,  false, true, false },
		{ "", "", "",
		  "", "", "",
		  TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
		  TextureTransformation::None, TextureTransformation::None, TextureTransformation::None });

	registerBlock(Block::GrassBlock,
		{ true, 0,  true,  false, true },
		{ "grass_block_side", "grass_block_side", "dirt",
		  "grass_block_top", "grass_block_side", "grass_block_side",
		  TextureTransformation::None, TextureTransformation::None, TextureTransformation::RotateAndFlip,
		  TextureTransformation::RotateAndFlip, TextureTransformation::None, TextureTransformation::None });

	registerBlock(Block::Dirt,
		{ true, 0,  true,  false, true },
		{ "dirt", "dirt", "dirt",
		  "dirt", "dirt", "dirt",
		  TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip,
		  TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip });

	registerBlock(Block::Stone,
		{ true, 0,  true,  false, true },
		{ "stone", "stone", "stone",
		  "stone", "stone", "stone",
		  TextureTransformation::Flip, TextureTransformation::Flip, TextureTransformation::Flip,
		  TextureTransformation::Flip, TextureTransformation::Flip, TextureTransformation::Flip });

	/*registerBlock(Block::GlowStone,
		{ true, 15, true,  false, true },
		{ "glowstone", "glowstone", "glowstone",
		  "glowstone", "glowstone", "glowstone",
		  TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip,
		  TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip });*/

	registerBlock(Block::Glass,
		{ false, 0, true,  true, true },
		{ "glass", "glass", "glass",
		  "glass", "glass", "glass",
		  TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
		  TextureTransformation::None, TextureTransformation::None, TextureTransformation::None });

	registerBlock(Block::ColoredGlass,
		{ false, 15, true,  true, true },
		{ "glass_red", "glass_green", "glass_blue",
		  "glass_cyan", "glass_pink", "glass_yellow",
		  TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
		  TextureTransformation::None, TextureTransformation::None, TextureTransformation::None });
}

const BlockData* BlockDataBase::getBlockData(Block block)
{
	return &BLOCK_DATABASE[(size_t)block];
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
	blockTexturesIDs = std::vector<BlockTextureIDs>((size_t)Block::__BlockCount__);
}

BlockTextureIDDatabase::~BlockTextureIDDatabase()
{
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

		IDs.ids[0] = strInd.getID(blockData->textures.texName_negativeX);
		IDs.ids[1] = strInd.getID(blockData->textures.texName_positiveX);
		IDs.ids[2] = strInd.getID(blockData->textures.texName_negativeY);
		IDs.ids[3] = strInd.getID(blockData->textures.texName_positiveY);
		IDs.ids[4] = strInd.getID(blockData->textures.texName_negativeZ);
		IDs.ids[5] = strInd.getID(blockData->textures.texName_positiveZ);
	}

	// Collect names
	textureNames.clear();
	const auto& nameToIDMap = strInd.getNameToIDMap();
	for (const auto& pair : nameToIDMap)
	{
		textureNames.push_back(pair.first);
	}

	// Sort names by index
	// TODO: Don't use map look-ups because we already have string with index above in pair
	std::sort(textureNames.begin(), textureNames.end(), [&nameToIDMap](const std::string& a, const std::string& b)
	{
		return nameToIDMap.find(a)->second < nameToIDMap.find(b)->second; // ascending order
	});
}

const BlockTextureIDDatabase::BlockTextureIDs& BlockTextureIDDatabase::getBlockTextureIDs(Block block) const
{
	return blockTexturesIDs[(size_t)block];
}
