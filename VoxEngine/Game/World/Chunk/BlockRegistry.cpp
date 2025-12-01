#include "BlockRegistry.h"

#include <algorithm>
#include <unordered_set>
#include <unordered_map>

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
std::unordered_map<size_t, BlockModelLoader::BlockModel> BlockRegistry::blockModelStorage;
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

	postBuild(textureNames);
	blockTempInfoStorage.clear();
}

void BlockRegistry::postBuild(std::vector<std::string>& textureNames)
{
	std::unordered_map<std::string, size_t> registeredTextures;
	size_t nextTextureID = 0;

	std::unordered_map<std::string, size_t> registeredModels;
	size_t nextModelID = 0;
	blockModelStorage.clear();

	for (auto& [blockID, blockData] : blockDataStorage)
	{
		const auto& tempInfo = blockTempInfoStorage.find(blockID)->second;

		// Set 'hasFaces'
		const bool hasFaces = !tempInfo.textureInfo.empty();
		blockData.properties.hasFaces = hasFaces;
		if (!hasFaces)
		{
			continue;
		}

		// Register textures
		for (const auto& [textureName, transformation, isTranslucent] : tempInfo.textureInfo)
		{
			size_t currentTextureID;
			auto it = registeredTextures.find(textureName);
			if (it == registeredTextures.end())
			{
				currentTextureID = nextTextureID++;
				registeredTextures.emplace(textureName, currentTextureID);
				textureNames.push_back(textureName);
			}
			else
			{
				currentTextureID = it->second;
			}
			blockData.visuals.textureSlots.emplace_back(currentTextureID, transformation, isTranslucent);
		}

		// Register model
		{
			size_t currentModelID;
			auto it = registeredModels.find(tempInfo.modelName);
			if (it == registeredModels.end())
			{
				auto loadingResult = BlockModelLoader::loadModelByName(tempInfo.modelName);
				currentModelID = nextModelID++;
				if (loadingResult.has_value())
				{
					blockModelStorage.emplace(currentModelID, loadingResult.value());
				}
				else
				{
					blockModelStorage.emplace(currentModelID, BlockModelLoader::BlockModel());
				}
				registeredModels.emplace(tempInfo.modelName, currentModelID);
			}
			else
			{
				currentModelID = it->second;
			}
			blockData.visuals.modelID = currentModelID;

			const auto& model = blockModelStorage.find(currentModelID)->second;

			// Enable culling for faces that have opaque aligned faces in the model
			for (int i = 0; i < 6; i++)
			{
				blockData.properties.faceCulling[i] = false;
			}
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
	return nullptr;
}

const BlockData* BlockRegistry::getBlockDataByID(BlockID id)
{
	auto it = blockDataStorage.find(id);
	if (it != blockDataStorage.end())
	{
		return &it->second;
	}
	return nullptr;
}

const BlockModelLoader::BlockModel* BlockRegistry::getBlockModelByID(size_t modelID)
{
	auto it = blockModelStorage.find(modelID);
	if (it != blockModelStorage.end())
	{
		return &it->second;
	}
	return nullptr;
}
