#include "BlockRegistry.h"
#include "SoundManager.h"

#include <algorithm>
#include <unordered_map>
#include <fstream>

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

TextureSlot::TextureSlot(uint32_t textureID, TextureInfo::TextureTransformation transformation, bool isTranslucent) :
	textureID(textureID), transformation(transformation), isTranslucent(isTranslucent)
{
}


BlockTempInfo::BlockTempInfo(
	const char* modelName,
	const std::vector<TextureInfo>& textureSlots
)
	:
	modelName(modelName),
	textureInfo(textureSlots)
{
}

BlockSounds::BlockSounds(
	const std::vector<std::string>& breakSounds,
	const std::vector<std::string>& placeSounds,
	const std::vector<std::string>& stepSounds
) :
	breakSounds(breakSounds),
	placeSounds(placeSounds),
	stepSounds(stepSounds)
{
}

BlockData::BlockData(const std::string& name, const BlockProperties& properties, const BlockVisuals& visuals, const BlockSounds& sounds) :
	name(name), properties(properties), visuals(visuals), sounds(sounds)
{
}


std::vector<BlockData> BlockRegistry::blockDataStorage;
std::vector<BlockTempInfo> BlockRegistry::blockTempInfoStorage;
std::vector<BlockModelLoader::BlockModel> BlockRegistry::blockModelStorage;
StringIndexer BlockRegistry::blockIndexer;
BlockID BlockRegistry::AIR_ID = 0;

void BlockRegistry::registerBlock(
	const std::string& blockName,
	const BlockProperties& properties,
	const BlockSounds& sounds,
	const BlockTempInfo& tempInfo
)
{
	if (blockIndexer.isRegistered(blockName))
	{
		// Block already registered, skip
		return;
	}

	blockIndexer.registerAndGetId(blockName);

	blockDataStorage.emplace_back( blockName, properties, BlockVisuals(), sounds );
	blockTempInfoStorage.emplace_back(tempInfo);
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
		const std::string& blockName = pair.key();
		/*size_t colonPos = blockName.find(':');
		if (
			colonPos == std::string::npos || colonPos == 0 || colonPos == blockName.size() - 1 ||
			colonPos < 3
			)
		{
			std::cerr << "[BlockRegistry]: Block name is invalid." << std::endl;
			continue;
		}*/

		const json& blockJson = pair.value();

		// Parse properties
		BlockProperties properties = parseJsonBlockProperties(blockJson);

		// Parse temp info
		BlockTempInfo tempInfo = parseJsonBlockTempInfo(blockJson);

		// Parse sounds
		BlockSounds sounds = parseJsonBlockSounds(blockJson);

		// Register block
		registerBlock(blockName, properties, sounds, tempInfo);
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
			TextureInfo::TextureTransformation transformation = TextureInfo::TextureTransformation::None;
			if (textureJson.contains("transformation"))
			{
				std::string transformationStr = textureJson["transformation"].get<std::string>();
				if (transformationStr == "None")
				{
					transformation = TextureInfo::TextureTransformation::None;
				}
				else if (transformationStr == "Flip")
				{
					transformation = TextureInfo::TextureTransformation::Flip;
				}
				else if (transformationStr == "RotateAndFlip")
				{
					transformation = TextureInfo::TextureTransformation::RotateAndFlip;
				}
			}

			// Is texture translucent
			bool isTranslucent = false;
			if (textureJson.contains("translucent"))
			{
				isTranslucent = textureJson["translucent"].get<bool>();
			}

			textureInfos.emplace_back(textureName, transformation, isTranslucent);
		}
	}
	return BlockTempInfo(modelName.c_str(), textureInfos);
}

BlockSounds BlockRegistry::parseJsonBlockSounds(const json& j)
{
	std::vector<std::string> breakSounds;
	std::vector<std::string> placeSounds;
	std::vector<std::string> stepSounds;

	// Break sounds
	if (j.contains("break_sounds") && j["break_sounds"].is_array())
	{
		for (const auto& soundJson : j["break_sounds"])
		{
			breakSounds.push_back(soundJson.get<std::string>());
		}
	}

	// Place sounds
	if (j.contains("place_sounds") && j["place_sounds"].is_array())
	{
		for (const auto& soundJson : j["place_sounds"])
		{
			placeSounds.push_back(soundJson.get<std::string>());
		}
	}

	// Step sounds
	if (j.contains("step_sounds") && j["step_sounds"].is_array())
	{
		for (const auto& soundJson : j["step_sounds"])
		{
			stepSounds.push_back(soundJson.get<std::string>());
		}
	}

	return BlockSounds(breakSounds, placeSounds, stepSounds);
}

void BlockRegistry::registerBlocks(
	std::vector<std::string>& textureNames
)
{
	registerBlocksFromFile("res/BlocksData/block_data.json");
	postBuild(textureNames);
	loadBlockSounds();
	blockTempInfoStorage.clear();

	AIR_ID = getBlockIDAirFallback("core:air");
	if (AIR_ID == (BlockID)-1)
	{
		throw std::runtime_error("[BlockRegistry][registerBlocks]: Air ID was not found.");
	}
}

void BlockRegistry::postBuild(std::vector<std::string>& textureNames)
{
	std::unordered_map<std::string, size_t> registeredTextures;
	size_t nextTextureID = 0;

	std::unordered_map<std::string, size_t> registeredModels;
	size_t nextModelID = 0;
	blockModelStorage.clear();

	// Reserve one, because it represents invalid block ID
	ASSERT(blockDataStorage.size() + 1 < 1 << (sizeof(BlockID) * 8));
	for (BlockID blockID = 0; blockID < blockDataStorage.size(); blockID++)
	{
		auto& blockData = blockDataStorage[blockID];
		const auto& tempInfo = blockTempInfoStorage[blockID];

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
					blockModelStorage.push_back(loadingResult.value());
				}
				else
				{
					blockModelStorage.push_back(BlockModelLoader::BlockModel());
				}
				registeredModels.emplace(tempInfo.modelName, currentModelID);
			}
			else
			{
				currentModelID = it->second;
			}
			blockData.visuals.modelID = currentModelID;

			const auto& model = blockModelStorage[currentModelID];

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

void BlockRegistry::loadBlockSounds()
{
	// TODO: Project have a lot of sound file duplicates. Optimize this later.
	auto& sndMgr = SoundManager::getInstance();
	for (BlockID blockID = 0; blockID < blockDataStorage.size(); blockID++)
	{
		auto& blockData = blockDataStorage[blockID];
		const auto& sounds = blockData.sounds;

		for (const auto& breakSound : sounds.breakSounds)
		{
			sndMgr.loadOgg("block/break/" + breakSound, "res/Sounds/Blocks/Break/" + breakSound + ".ogg");
		}
		for (const auto& placeSound : sounds.placeSounds)
		{
			sndMgr.loadOgg("block/place/" + placeSound, "res/Sounds/Blocks/Place/" + placeSound + ".ogg");
		}
		for (const auto& stepSound : sounds.stepSounds)
		{
			sndMgr.loadOgg("block/step/" + stepSound, "res/Sounds/Blocks/Step/" + stepSound + ".ogg");
		}
	}
}

BlockID BlockRegistry::getBlockID(const std::string& blockName)
{
	auto result = blockIndexer.getId(blockName);
	if (result.has_value()) return result.value();
	return -1;
}

BlockID BlockRegistry::getBlockIDAirFallback(const std::string& blockName)
{
	auto result = blockIndexer.getId(blockName);
	if (result.has_value()) return result.value();
	return AIR_ID;
}

const BlockData* BlockRegistry::getBlockDataByName(const std::string& blockName)
{
	auto result = blockIndexer.getId(blockName);
	if (!result.has_value())
	{
		return nullptr;
	}
	return getBlockDataByID(result.value());
}

const BlockData* BlockRegistry::getBlockDataByID(BlockID id)
{
	if (id >= blockDataStorage.size())
	{
		return nullptr;
	}
	return &blockDataStorage[id];
}

const BlockModelLoader::BlockModel* BlockRegistry::getBlockModelByID(size_t modelID)
{
	if (modelID >= blockModelStorage.size())
	{
		return nullptr;
	}
	return &blockModelStorage[modelID];
}
