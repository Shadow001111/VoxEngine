#include "BlockRegistry.h"
#include "BlockModelLoader.h"

#include <algorithm>

#include "Core/Assert.h"

inline uint8_t clamp(uint8_t v, uint8_t min, uint8_t max)
{
	return std::min(max, std::max(min, v));
}

BlockProperties::BlockProperties(bool absorbsLight, uint8_t lightEmission, bool raycastable) :
	absorbsLight(absorbsLight),
	lightEmission(clamp(lightEmission, 0, 15)),
	raycastable(raycastable)
{}

TextureInfo::TextureInfo(const std::string& name, TextureTransformation transformation, bool isTranslucent) :
	name(name), transformation(transformation), isTranslucent(isTranslucent)
{
}

TextureSlot::TextureSlot(uint32_t textureID, TextureTransformation transformation, bool isTranslucent) :
	textureID(textureID), transformation(transformation), isTranslucent(isTranslucent)
{
}


BlockTempInfo::BlockTempInfo(
	const char* modelName,
	const std::vector<TextureInfo>& textureSlots)
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

	blockDataStorage[blockID] = { properties, BlockVisuals() };
	blockTempInfoStorage[blockID] = tempInfo;
}

void BlockRegistry::registerBlocks(
	std::vector<std::string>& textureNames
)
{
	std::vector<std::string> modelNames;

	registerBlock("core:air",
		{ false,  0,false },
		{ "", {} }
	);

	registerBlock("core:grass_block",
		{ true, 0, true },
		{ "cube", {
			{"grass_block_side", TextureTransformation::None		 , false},
			{"grass_block_side", TextureTransformation::None		 , false},
			{"dirt",			 TextureTransformation::RotateAndFlip, false},
			{"grass_block_top",  TextureTransformation::RotateAndFlip, false},
			{"grass_block_side", TextureTransformation::None		 , false},
			{"grass_block_side", TextureTransformation::None		 , false}
		} }
	);

	registerBlock("core:dirt",
		{ true, 0, true },
		{ "cube", {
			{"dirt", TextureTransformation::RotateAndFlip, false},
			{"dirt", TextureTransformation::RotateAndFlip, false},
			{"dirt", TextureTransformation::RotateAndFlip, false},
			{"dirt", TextureTransformation::RotateAndFlip, false},
			{"dirt", TextureTransformation::RotateAndFlip, false},
			{"dirt", TextureTransformation::RotateAndFlip, false}
		} }
	);

	registerBlock("core:stone",
		{ true, 0, true },
		{ "cube", {
			{"stone", TextureTransformation::Flip, false},
			{"stone", TextureTransformation::Flip, false},
			{"stone", TextureTransformation::Flip, false},
			{"stone", TextureTransformation::Flip, false},
			{"stone", TextureTransformation::Flip, false},
			{"stone", TextureTransformation::Flip, false}
		} }
	);

	registerBlock("core:glass",
		{ false, 0, true },
		{ "cube", {
			{"glass", TextureTransformation::None, true},
			{"glass", TextureTransformation::None, true},
			{"glass", TextureTransformation::None, true},
			{"glass", TextureTransformation::None, true},
			{"glass", TextureTransformation::None, true},
			{"glass", TextureTransformation::None, true}
		} }
	);

	registerBlock("core:colored_glass",
		{ false, 15, true },
		{ "cube", {
			{"glass_red", TextureTransformation::None  , true},
			{"glass_red", TextureTransformation::None  , true},
			{"glass_green", TextureTransformation::None, true},
			{"glass_green", TextureTransformation::None, true},
			{"glass_blue", TextureTransformation::None , true},
			{"glass_blue", TextureTransformation::None , true}
		} }
	);

	registerBlock("core:water",
		{ false, 0, false },
		{ "cube", {
			{"water", TextureTransformation::None, true},
			{"water", TextureTransformation::None, true},
			{"water", TextureTransformation::None, true},
			{"water", TextureTransformation::None, true},
			{"water", TextureTransformation::None, true},
			{"water", TextureTransformation::None, true}
		} }
	);

	registerBlock("core:log_oak",
		{ true, 0, true },
		{ "cube", {
			{"log_oak", TextureTransformation::None	   , false},
			{"log_oak", TextureTransformation::None	   , false},
			{"log_oak_top", TextureTransformation::None, false},
			{"log_oak_top", TextureTransformation::None, false},
			{"log_oak", TextureTransformation::None	   , false},
			{"log_oak", TextureTransformation::None	   , false}
		} }
	);

	registerBlock("core:leaves_oak",
		{ false, 0, true },
		{ "cube", {
			{"leaves_oak", TextureTransformation::None, true},
			{"leaves_oak", TextureTransformation::None, true},
			{"leaves_oak", TextureTransformation::None, true},
			{"leaves_oak", TextureTransformation::None, true},
			{"leaves_oak", TextureTransformation::None, true},
			{"leaves_oak", TextureTransformation::None, true}
		} }
	);

	registerBlock("core:stairs",
		{ true, 0, true },
		{ "stairs", {
			{"stone", TextureTransformation::None, false}
		} }
	);

	buildIDs(textureNames, modelNames);
	blockTempInfoStorage.clear();

	BlockModelLoader::loadModels(modelNames);

	updateBlockPropertiesBasedOnModels();
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
		const auto& tempInfo = blockTempInfoStorage.find(blockID)->second;

		blockData.properties.hasFaces = !tempInfo.textureInfo.empty();

		if (!blockData.properties.hasFaces)
		{
			continue;
		}

		blockData.visuals.modelID = modelIndexer.registerAndGetId(tempInfo.modelName);

		for (const auto& [textureName, transformation, isTranslucent] : tempInfo.textureInfo)
		{
			auto textureID = textureIndexer.registerAndGetId(textureName);
			blockData.visuals.textureSlots.emplace_back(textureID, transformation, isTranslucent);
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

void BlockRegistry::updateBlockPropertiesBasedOnModels()
{
	for (auto& [blockID, blockData] : blockDataStorage)
	{
		// Skip blocks without faces
		if (!blockData.properties.hasFaces)
		{
			continue;
		}

		// Get the block's model
		const auto& model = BlockModelLoader::getBlockModelById(blockData.visuals.modelID);

		// Reset all face culling to false initially
		for (int i = 0; i < 6; i++)
		{
			blockData.properties.faceCulling[i] = false;
		}

		// Enable culling for faces that have opaque aligned faces in the model
		for (const auto& alignedFace : model.alignedFaces)
		{
			if (alignedFace.normal >= 6)
			{
				continue;
			}

			bool shouldCull = true;

			if (alignedFace.textureSlot < blockData.visuals.textureSlots.size())
			{
				const auto& textureSlot = blockData.visuals.textureSlots[alignedFace.textureSlot];
				if (textureSlot.isTranslucent)
				{
					shouldCull = false;
				}
			}

			blockData.properties.faceCulling[alignedFace.normal] = shouldCull;
		}
	}
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