#include "BlockData.h"

#include "Core/StringIndexer.h"

#include <algorithm>

inline uint8_t clamp(uint8_t v, uint8_t min, uint8_t max)
{
	return std::min(max, std::max(min, v));
}

inline uint16_t packTransformations(
	TextureTransformation nx, TextureTransformation px,
	TextureTransformation ny, TextureTransformation py,
	TextureTransformation nz, TextureTransformation pz)
{
	uint16_t result = 0u;
	result |= uint16_t(nx);
	result |= uint16_t(px) << 2u;
	result |= uint16_t(ny) << 4u;
	result |= uint16_t(py) << 6u;
	result |= uint16_t(nz) << 8u;
	result |= uint16_t(pz) << 10u;
	return result;
}


BlockProperties::BlockProperties(bool absorbsLight, uint8_t lightEmission, bool hasFaces, bool areFacesTransparent, bool raycastable) :
	absorbsLight(absorbsLight),
	lightEmission(clamp(lightEmission, 0, 15)),
	hasFaces(hasFaces),
	areFacesTransparent(areFacesTransparent || !hasFaces),
	raycastable(raycastable)
{
}

BlockTextures::BlockTextures(uint16_t texturesTransformation) :
	texturesTransformation(texturesTransformation)
{
}

BlockTextureNames::BlockTextureNames(
	const char* nxName, const char* pxName,
	const char* nyName, const char* pyName,
	const char* nzName, const char* pzName)
	:
	texName_negativeX(nxName), texName_positiveX(pxName),
	texName_negativeY(nyName), texName_positiveY(pyName),
	texName_negativeZ(nzName), texName_positiveZ(pzName)
{
}

BlockData::BlockData(const BlockProperties& properties, const BlockTextures& textures) :
	properties(properties), textures(textures)
{
}


BlockData BlockDataBase::BLOCK_DATABASE[(size_t)Block::__BlockCount__];
BlockTextureNames BlockDataBase::TEXTURE_NAMES[(size_t)Block::__BlockCount__];

void BlockDataBase::registerBlock(Block block,
	const BlockProperties& properties,
	const BlockTextureNames& textureNames,
	uint16_t texturesTransformation)
{
	BLOCK_DATABASE[(size_t)block] = { properties, BlockTextures(texturesTransformation) };
	TEXTURE_NAMES[(size_t)block] = textureNames;
}

// TODO: Import block data from some file. Store 'compiled' file in binary for fast loading. Check if file was updates by hashing.
void BlockDataBase::loadBlockDataBase(std::vector<std::string>& textureNames)
{
	registerBlock(Block::Air,
		{ false,  0,  false, true, false },
		{ "", "", "", "", "", "" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock(Block::GrassBlock,
		{ true, 0,  true,  false, true },
		{ "grass_block_side", "grass_block_side", "dirt", "grass_block_top", "grass_block_side", "grass_block_side" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::RotateAndFlip,
			TextureTransformation::RotateAndFlip, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock(Block::Dirt,
		{ true, 0,  true,  false, true },
		{ "dirt", "dirt", "dirt", "dirt", "dirt", "dirt" },
		packTransformations(
			TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip,
			TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip)
	);

	registerBlock(Block::Stone,
		{ true, 0,  true,  false, true },
		{ "stone", "stone", "stone", "stone", "stone", "stone" },
		packTransformations(
			TextureTransformation::Flip, TextureTransformation::Flip, TextureTransformation::Flip,
			TextureTransformation::Flip, TextureTransformation::Flip, TextureTransformation::Flip)
	);

	registerBlock(Block::Glass,
		{ false, 0, true,  true, true },
		{ "glass", "glass", "glass", "glass", "glass", "glass" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock(Block::ColoredGlass,
		{ false, 15, true,  true, true },
		{ "glass_red", "glass_red", "glass_green", "glass_green", "glass_blue", "glass_blue" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock(Block::Water,
		{ false, 0, true,  true, false },
		{ "water", "water", "water", "water", "water", "water" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock(Block::LogOak,
		{ true, 0,  true,  false, true },
		{ "log_oak", "log_oak", "log_oak_top", "log_oak_top", "log_oak", "log_oak" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock(Block::LeavesOak,
		{ false, 0,  true,  true, true },
		{ "leaves_oak", "leaves_oak", "leaves_oak", "leaves_oak", "leaves_oak", "leaves_oak" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	BlockDataBase::buildTextureIDs(textureNames);
}

void BlockDataBase::buildTextureIDs(std::vector<std::string>& textureNames)
{
	StringIndexer strInd;

	const size_t blockCount = (size_t)Block::__BlockCount__;

	// First pass: index all texture names
	for (size_t blockID = 0; blockID < blockCount; blockID++)
	{
		const BlockData* blockData = &BLOCK_DATABASE[blockID];
		if (!blockData->properties.hasFaces)
		{
			continue;
		}

		const BlockTextureNames& names = TEXTURE_NAMES[blockID];
		strInd.getID(names.texName_negativeX);
		strInd.getID(names.texName_positiveX);
		strInd.getID(names.texName_negativeY);
		strInd.getID(names.texName_positiveY);
		strInd.getID(names.texName_negativeZ);
		strInd.getID(names.texName_positiveZ);
	}

	// Second pass: assign IDs to BlockTextures
	for (size_t blockID = 0; blockID < blockCount; blockID++)
	{
		BlockData* blockData = &BLOCK_DATABASE[blockID];
		if (!blockData->properties.hasFaces)
		{
			continue;
		}

		const BlockTextureNames& names = TEXTURE_NAMES[blockID];
		blockData->textures.textureIDs[0] = strInd.getID(names.texName_negativeX);
		blockData->textures.textureIDs[1] = strInd.getID(names.texName_positiveX);
		blockData->textures.textureIDs[2] = strInd.getID(names.texName_negativeY);
		blockData->textures.textureIDs[3] = strInd.getID(names.texName_positiveY);
		blockData->textures.textureIDs[4] = strInd.getID(names.texName_negativeZ);
		blockData->textures.textureIDs[5] = strInd.getID(names.texName_positiveZ);
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
			return nameToIDMap.find(a)->second < nameToIDMap.find(b)->second;
		});
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