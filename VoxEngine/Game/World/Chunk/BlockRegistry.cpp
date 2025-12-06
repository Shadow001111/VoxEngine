#include "BlockRegistry.h"

#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <algorithm>

#include "Core/Assert.h"

inline uint8_t clamp(uint8_t v, uint8_t min, uint8_t max)
{
	return std::min(max, std::max(min, v));
}

bool is_ascii(const std::string& s)
{
	return std::all_of(s.begin(), s.end(),
		[](unsigned char c) { return c < 128; });
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

void BlockRegistry::registerBlocksFromFile(const std::filesystem::path& filepath)
{
	// Check if file exists
	if (!std::filesystem::exists(filepath))
	{
		throw std::runtime_error("[BlockRegistry]: File not found: " + filepath.string());
		return;
	}

	// Open file
	std::ifstream file(filepath);
	if (!file.is_open())
	{
		throw std::runtime_error("[BlockRegistry]: Failed to open block names file: " + filepath.string());
	}

	try
	{
		json j;
		file >> j;
		registerBlocksFromJson(j);
	}
	catch (const json::exception& e)
	{
		std::cerr << "[BlockRegistry]: JSON parsing error in file " << filepath << ": " << e.what() << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "[BlockRegistry]: Error reading file " << filepath << ": " << e.what() << std::endl;
	}
}

void BlockRegistry::registerBlocksFromJson(const json& j)
{
	for (auto pair : j.items())
	{
		const std::string& blockName = "core:" + pair.key();
		const json& blockJson = pair.value();

		// Parse properties
		BlockProperties properties = parseJsonBlockProperties(blockJson);

		// Parse temp info
		BlockTempInfo tempInfo = parseJsonBlockTempInfo(blockJson);

		// Register block
		registerBlock(blockName, properties, tempInfo);
	}
}

BlockProperties BlockRegistry::parseJsonBlockProperties(const json& j)
{
	bool absorbsLight = false;
	uint8_t lightEmission = 0;
	bool raycastable = true;

	if (j.contains("absorbs_light"))
	{
		absorbsLight = j["absorbs_light"].get<bool>();
	}
	else
	{
		std::cerr << "[BlockRegistry]: Block properties missing absorbs_light field" << std::endl;
	}

	if (j.contains("light_emission"))
	{
		lightEmission = j["light_emission"].get<uint8_t>();
	}
	else
	{
		std::cerr << "[BlockRegistry]: Block properties missing light_emission field" << std::endl;
	}

	if (j.contains("raycastable"))
	{
		raycastable = j["raycastable"].get<bool>();
	}
	else
	{
		std::cerr << "[BlockRegistry]: Block properties missing raycastable field" << std::endl;
	}

	BlockProperties properties(absorbsLight, lightEmission, raycastable);
	return properties;
}

BlockTempInfo BlockRegistry::parseJsonBlockTempInfo(const json& j)
{
	std::string modelName;
	std::vector<TextureInfo> textureInfos;

	// Model name
	if (j.contains("model"))
	{
		modelName = j["model"].get<std::string>();
	}
	else
	{
		std::cerr << "[BlockRegistry]: Block visuals missing model field" << std::endl;
	}

	// Textures
	if (j.contains("textures") && j["textures"].is_array())
	{
		for (const auto& textureJson : j["textures"])
		{
			// Texture name
			std::string textureName;
			if (textureJson.contains("name"))
			{
				textureName = textureJson["name"].get<std::string>();
			}
			else
			{
				std::cerr << "[BlockRegistry]: Block texture missing name field" << std::endl;
				continue;
			}

			// Texture transformation
			TextureTransformation transformation = TextureTransformation::None;
			if (textureJson.contains("transformation"))
			{
				std::string transformationStr = textureJson["transformation"].get<std::string>();
				if (transformationStr == "None")
				{
					transformation = TextureTransformation::None;
				}
				else if (transformationStr == "Flip")
				{
					transformation = TextureTransformation::Flip;
				}
				else if (transformationStr == "RotateAndFlip")
				{
					transformation = TextureTransformation::RotateAndFlip;
				}
				else
				{
					std::cerr << "[BlockRegistry]: Unknown texture transformation: " << transformationStr << std::endl;
				}
			}

			// Is texture translucent
			bool isTranslucent = false;
			if (textureJson.contains("translucent"))
			{
				isTranslucent = textureJson["translucent"].get<bool>();
			}
			else
			{
				std::cerr << "[BlockRegistry]: Block texture missing translucent field" << std::endl;
				continue;
			}
			textureInfos.emplace_back(textureName, transformation, isTranslucent);
		}
	}
	else
	{
		std::cerr << "[BlockRegistry]: Block visuals missing textures array" << std::endl;
	}
	return BlockTempInfo(modelName.c_str(), textureInfos);
}

void BlockRegistry::registerBlocks(
	std::vector<std::string>& textureNames
)
{
	registerBlocksFromFile("res/BlocksData/block_data.json");
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
		// TODO: Support blocks without models (like air)
		//if (!tempInfo.modelName.empty())
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
	return -1;
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
