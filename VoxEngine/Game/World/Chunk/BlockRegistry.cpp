#include "BlockRegistry.h"

#include <algorithm>
#include <stdexcept>

#include "Core/Assert.h"

inline uint8_t clamp(uint8_t v, uint8_t min, uint8_t max)
{
	return std::min(max, std::max(min, v));
}

BlockProperties::BlockProperties(bool absorbsLight, uint8_t lightEmission, bool hasFaces, bool areFacesTranslucent, bool raycastable) :
	absorbsLight(absorbsLight),
	lightEmission(clamp(lightEmission, 0, 15)),
	hasFaces(hasFaces),
	areFacesTranslucent(areFacesTranslucent || !hasFaces),
	raycastable(raycastable)
{
}

BlockTempInfo::BlockTempInfo(
	const char* modelName,
	const std::vector<std::pair<std::string, TextureTransformation>>& textureSlots)
	:
	modelName(modelName),
	textureInfo(textureSlots)
{
}

BlockData::BlockData(const BlockProperties& properties, const BlockVisuals& textures) :
	properties(properties), visuals(textures)
{
}

std::unordered_map<BlockID, BlockData> BlockRegistry::blockDataStorage;
std::unordered_map<BlockID, BlockTempInfo> BlockRegistry::blockTempInfoStorage;
StringIndexer BlockRegistry::blockIndexer;

void BlockRegistry::registerBlock(const std::string& blockName, const BlockProperties& properties, const BlockTempInfo& tempInfo)
{
	if (blockIndexer.isRegistered(blockName))
	{
		// Block already registered, skip
		return;
	}

	BlockID blockID = static_cast<BlockID>(blockIndexer.registerAndGetId(blockName));
	
	std::vector<std::pair<uint32_t, TextureTransformation>> textureSlots;
	for (const auto& [textureName, transformation] : tempInfo.textureInfo)
	{
		textureSlots.emplace_back(0, transformation); // ID will be set in buildIDs
	}

	blockDataStorage[blockID] = { properties, BlockVisuals() };
	blockTempInfoStorage[blockID] = tempInfo;
}

void BlockRegistry::registerBlocks(
	std::vector<std::string>& textureNames,
	std::vector<std::string>& modelNames
)
{
	registerBlock("core:air",
		{ false,  0,  false, true, false },
		{ "", {} }
	);

	registerBlock("core:grass_block",
		{ true, 0,  true,  false, true },
		{ "cube", {
			{"grass_block_side", TextureTransformation::None},
			{"grass_block_side", TextureTransformation::None},
			{"dirt", TextureTransformation::RotateAndFlip},
			{"grass_block_top", TextureTransformation::RotateAndFlip},
			{"grass_block_side", TextureTransformation::None},
			{"grass_block_side", TextureTransformation::None}
		} }
	);

	registerBlock("core:dirt",
		{ true, 0,  true,  false, true },
		{ "cube", {
			{"dirt", TextureTransformation::RotateAndFlip},
			{"dirt", TextureTransformation::RotateAndFlip},
			{"dirt", TextureTransformation::RotateAndFlip},
			{"dirt", TextureTransformation::RotateAndFlip},
			{"dirt", TextureTransformation::RotateAndFlip},
			{"dirt", TextureTransformation::RotateAndFlip}
		} }
	);

	registerBlock("core:stone",
		{ true, 0,  true,  false, true },
		{ "cube", {
			{"stone", TextureTransformation::Flip},
			{"stone", TextureTransformation::Flip},
			{"stone", TextureTransformation::Flip},
			{"stone", TextureTransformation::Flip},
			{"stone", TextureTransformation::Flip},
			{"stone", TextureTransformation::Flip}
		} }
	);

	registerBlock("core:glass",
		{ false, 0, true,  true, true },
		{ "cube", {
			{"glass", TextureTransformation::None},
			{"glass", TextureTransformation::None},
			{"glass", TextureTransformation::None},
			{"glass", TextureTransformation::None},
			{"glass", TextureTransformation::None},
			{"glass", TextureTransformation::None}
		} }
	);

	registerBlock("core:colored_glass",
		{ false, 15, true,  true, true },
		{ "cube", {
			{"glass_red", TextureTransformation::None},
			{"glass_red", TextureTransformation::None},
			{"glass_green", TextureTransformation::None},
			{"glass_green", TextureTransformation::None},
			{"glass_blue", TextureTransformation::None},
			{"glass_blue", TextureTransformation::None}
		} }
	);

	registerBlock("core:water",
		{ false, 0, true,  true, false },
		{ "cube", {
			{"water", TextureTransformation::None},
			{"water", TextureTransformation::None},
			{"water", TextureTransformation::None},
			{"water", TextureTransformation::None},
			{"water", TextureTransformation::None},
			{"water", TextureTransformation::None}
		} }
	);

	registerBlock("core:log_oak",
		{ true, 0,  true,  false, true },
		{ "cube", {
			{"log_oak", TextureTransformation::None},
			{"log_oak", TextureTransformation::None},
			{"log_oak_top", TextureTransformation::None},
			{"log_oak_top", TextureTransformation::None},
			{"log_oak", TextureTransformation::None},
			{"log_oak", TextureTransformation::None}
		} }
	);

	registerBlock("core:leaves_oak",
		{ false, 0,  true,  true, true },
		{ "cube", {
			{"leaves_oak", TextureTransformation::None},
			{"leaves_oak", TextureTransformation::None},
			{"leaves_oak", TextureTransformation::None},
			{"leaves_oak", TextureTransformation::None},
			{"leaves_oak", TextureTransformation::None},
			{"leaves_oak", TextureTransformation::None}
		} }
	);

	registerBlock("core:stairs",
		{ true, 0,  true,  false, true },
		{ "stairs", {
			{"stone", TextureTransformation::None}
		} }
	);

	BlockRegistry::buildIDs(textureNames, modelNames);
	blockTempInfoStorage.clear();
}

void BlockRegistry::buildIDs(
	std::vector<std::string>& textureNames,
	std::vector<std::string>& modelNames
)
{
	StringIndexer textureIndexer;
	StringIndexer modelIndexer;

	// Assign IDs
	//textureIndexer.registerAndGetId(""); // Broken texture
	for (auto& [blockID, blockData] : blockDataStorage)
	{
		// TODO: Add check for empty string
		if (!blockData.properties.hasFaces)
		{
			continue;
		}

		const auto& tempInfo = blockTempInfoStorage.find(blockID)->second;

		blockData.visuals.modelID = modelIndexer.registerAndGetId(tempInfo.modelName);

		for (const auto& [textureName, transformation] : tempInfo.textureInfo)
		{
			auto textureID = textureIndexer.registerAndGetId(textureName);
			blockData.visuals.textureSlots.emplace_back(textureID, transformation);
		}
	}

	// Collect and sort
	{
		textureNames.clear();
		const auto& nameToIDMap = textureIndexer.getNameToIDMap();
		for (const auto& pair : nameToIDMap)
		{
			textureNames.push_back(pair.first);
		}
		std::sort(textureNames.begin(), textureNames.end(), [&nameToIDMap](const std::string& a, const std::string& b)
			{
				return nameToIDMap.find(a)->second < nameToIDMap.find(b)->second;
			});
	}

	{
		modelNames.clear();
		const auto& nameToIDMap = modelIndexer.getNameToIDMap();
		for (const auto& pair : nameToIDMap)
		{
			modelNames.push_back(pair.first);
		}
		std::sort(modelNames.begin(), modelNames.end(), [&nameToIDMap](const std::string& a, const std::string& b)
			{
				return nameToIDMap.find(a)->second < nameToIDMap.find(b)->second;
			});
	}

	// TODO: Find better way to sort this, without look ups. Maybe flatten and the sort then collect?
}

BlockID BlockRegistry::getBlockID(const std::string& blockName)
{
	auto result = blockIndexer.getId(blockName);
	if (result.has_value()) return result.value();
	return 0;
}

const BlockData* BlockRegistry::getBlockDataByName(const std::string& blockName)
{
	auto result = blockIndexer.getId(blockName);
	if (result.has_value())
	{
		return &blockDataStorage[result.value()];
	}
	return &blockDataStorage[0];
}

const BlockData* BlockRegistry::getBlockDataByID(BlockID id)
{
	auto it = blockDataStorage.find(id);
	if (it != blockDataStorage.end())
	{
		return &it->second;
	}
	return &blockDataStorage[0];
}
