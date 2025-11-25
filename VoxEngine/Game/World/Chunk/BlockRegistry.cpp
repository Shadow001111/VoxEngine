#include "BlockRegistry.h"

#include <algorithm>
#include <stdexcept>

#include "Core/Assert.h"

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

std::unordered_map<BlockID, BlockData> BlockRegistry::BLOCK_DATABASE;
std::unordered_map<BlockID, BlockTextureNames> BlockRegistry::TEXTURE_NAMES;
StringIndexer BlockRegistry::blockIndexer;

void BlockRegistry::registerBlock(const std::string& blockName, const BlockProperties& properties, const BlockTextureNames& textureNames, uint16_t texturesTransformation)
{
	if (blockIndexer.isRegistered(blockName))
	{
		// Block already registered, skip
		return;
	}

	// Maybe StringIndexer is unnecessary thing...
	BlockID blockID = static_cast<BlockID>(blockIndexer.registerAndGetId(blockName));
	BLOCK_DATABASE[blockID] = { properties, BlockTextures(texturesTransformation) };
	TEXTURE_NAMES[blockID] = textureNames;
}

// TODO: Import block data from some file. Store 'compiled' file in binary for fast loading. Check if file was updates by hashing.
void BlockRegistry::registerBlocks(std::vector<std::string>& textureNames)
{
	registerBlock("core:air",
		{ false,  0,  false, true, false },
		{ "", "", "", "", "", "" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock("core:grass_block",
		{ true, 0,  true,  false, true },
		{ "grass_block_side", "grass_block_side", "dirt", "grass_block_top", "grass_block_side", "grass_block_side" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::RotateAndFlip,
			TextureTransformation::RotateAndFlip, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock("core:dirt",
		{ true, 0,  true,  false, true },
		{ "dirt", "dirt", "dirt", "dirt", "dirt", "dirt" },
		packTransformations(
			TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip,
			TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip, TextureTransformation::RotateAndFlip)
	);

	registerBlock("core:stone",
		{ true, 0,  true,  false, true },
		{ "stone", "stone", "stone", "stone", "stone", "stone" },
		packTransformations(
			TextureTransformation::Flip, TextureTransformation::Flip, TextureTransformation::Flip,
			TextureTransformation::Flip, TextureTransformation::Flip, TextureTransformation::Flip)
	);

	registerBlock("core:glass",
		{ false, 0, true,  true, true },
		{ "glass", "glass", "glass", "glass", "glass", "glass" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock("core:colored_glass",
		{ false, 15, true,  true, true },
		{ "glass_red", "glass_red", "glass_green", "glass_green", "glass_blue", "glass_blue" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock("core:water",
		{ false, 0, true,  true, false },
		{ "water", "water", "water", "water", "water", "water" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock("core:log_oak",
		{ true, 0,  true,  false, true },
		{ "log_oak", "log_oak", "log_oak_top", "log_oak_top", "log_oak", "log_oak" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	registerBlock("core:leaves_oak",
		{ false, 0,  true,  true, true },
		{ "leaves_oak", "leaves_oak", "leaves_oak", "leaves_oak", "leaves_oak", "leaves_oak" },
		packTransformations(
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None,
			TextureTransformation::None, TextureTransformation::None, TextureTransformation::None)
	);

	BlockRegistry::buildTextureIDs(textureNames);
}

void BlockRegistry::buildTextureIDs(std::vector<std::string>& textureNames)
{
	StringIndexer strInd;

	// First pass: index all texture names
	for (auto& [blockID, blockData] : BLOCK_DATABASE)
	{
		if (!blockData.properties.hasFaces)
		{
			continue;
		}

		const BlockTextureNames& names = TEXTURE_NAMES[blockID];
		strInd.registerAndGetId(names.texName_negativeX);
		strInd.registerAndGetId(names.texName_positiveX);
		strInd.registerAndGetId(names.texName_negativeY);
		strInd.registerAndGetId(names.texName_positiveY);
		strInd.registerAndGetId(names.texName_negativeZ);
		strInd.registerAndGetId(names.texName_positiveZ);
	}

	// Second pass: assign IDs to BlockTextures
	for (auto& [blockID, blockData] : BLOCK_DATABASE)
	{
		if (!blockData.properties.hasFaces)
		{
			continue;
		}

		const BlockTextureNames& names = TEXTURE_NAMES[blockID];
		blockData.textures.textureIDs[0] = strInd.registerAndGetId(names.texName_negativeX);
		blockData.textures.textureIDs[1] = strInd.registerAndGetId(names.texName_positiveX);
		blockData.textures.textureIDs[2] = strInd.registerAndGetId(names.texName_negativeY);
		blockData.textures.textureIDs[3] = strInd.registerAndGetId(names.texName_positiveY);
		blockData.textures.textureIDs[4] = strInd.registerAndGetId(names.texName_negativeZ);
		blockData.textures.textureIDs[5] = strInd.registerAndGetId(names.texName_positiveZ);
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

BlockID BlockRegistry::getBlockID(const std::string& blockName)
{
	auto result = blockIndexer.getId(blockName);
	if (result.has_value()) return result.value();
	ASSERT(false);
	return 0;
}

const BlockData* BlockRegistry::getBlockDataByName(const std::string& blockName)
{
	auto result = blockIndexer.getId(blockName);
	if (result.has_value())
	{
		return &BLOCK_DATABASE[result.value()];
	}
	ASSERT(false);

	auto fallbackId = blockIndexer.getId("core:air");
	if (fallbackId.has_value())
	{
		return &BLOCK_DATABASE[fallbackId.value()];
	}

	throw std::runtime_error("[BlockRegistry]: BlockName: " + blockName + " doesn't exist.");
}

const BlockData* BlockRegistry::getBlockDataByID(BlockID id)
{
	auto it = BLOCK_DATABASE.find(id);
	if (it != BLOCK_DATABASE.end())
	{
		return &it->second;
	}
	ASSERT(false);
	
	auto fallbackId = blockIndexer.getId("core:air");
	if (!fallbackId.has_value())
	{
		throw std::runtime_error("[BlockRegistry]: Id: " + std::to_string(static_cast<size_t>(id)) + " doesn't exist.");
	}

	return &BLOCK_DATABASE[fallbackId.value()];
}
