#include "AssetRegistry.h"
#include <iostream>
#include <algorithm>
#include "SoundManager.h"

std::vector<BlockAsset> AssetRegistry::blockAssetStorage;

std::vector<BlockData> AssetRegistry::blockDataStorage;
std::vector<BlockModelData> AssetRegistry::blockModelDataStorage;

StringIndexer AssetRegistry::blockIndexer;
StringIndexer AssetRegistry::blockModelIndexer;
StringIndexer AssetRegistry::blockTextureIndexer;

BlockId AssetRegistry::FALLBACK_BLOCK_ID;
BlockModelId AssetRegistry::FALLBACK_BLOCK_MODEL_ID;

// Object name validation
ObjectNameValidationResult validateObjectName(std::string_view name) noexcept
{
    // Check size
    if (name.size() == 0)
    {
        return ObjectNameValidationResult::Empty;
    }
    else if (name.size() > MAX_OBJECT_NAME_SIZE)
    {
        return ObjectNameValidationResult::TooLong_Name;
    }

    // Check if ASCII and for colon
    for (size_t i = 0; i < name.length(); ++i)
    {
        char c = name[i];

        if (c < ' ')
        {
            return ObjectNameValidationResult::NonAscii;
        }
        else if (c == ':')
        {
            return ObjectNameValidationResult::ContainsColon;
        }
    }

    return ObjectNameValidationResult::Success;
}

ObjectNameValidationResult validateObjectStringId(std::string_view stringId) noexcept
{
    // Check size
    if (stringId.size() == 0)
    {
        return ObjectNameValidationResult::Empty;
    }
    else if (stringId.size() > MAX_OBJECT_NAME_SIZE * 2 + 1)
    {
        return ObjectNameValidationResult::TooLong_Id;
    }

    // Check if ASCII and colon
    bool foundColon = false;
    size_t colonPos = std::string::npos;
    for (size_t i = 0; i < stringId.size(); ++i)
    {
        char c = stringId[i];

        if (c < ' ')
        {
            return ObjectNameValidationResult::NonAscii;
        }
        else if (c == ':')
        {
            if (foundColon)
            {
                return ObjectNameValidationResult::ContainsMoreThanOneColon;
            }

            foundColon = true;
            colonPos = i;
        }
    }
    if (!foundColon)
    {
        return ObjectNameValidationResult::MissingColon;
    }

    // Extract both parts
    std::string_view packName = stringId.substr(0, colonPos);
    std::string_view objectName = stringId.substr(colonPos + 1);

    // Check if both parts have valid length
    if (packName.size() == 0)
    {
        return ObjectNameValidationResult::PackNameIsEmpty;
    }
    else if (packName.size() > MAX_OBJECT_NAME_SIZE)
    {
        return ObjectNameValidationResult::PackNameIsTooLong;
    }

    if (objectName.size() == 0)
    {
        return ObjectNameValidationResult::ObjectNameisEmpty;
    }
    else if (objectName.size() > MAX_OBJECT_NAME_SIZE)
    {
        return ObjectNameValidationResult::ObjectNameIsTooLong;
    }

    return ObjectNameValidationResult::Success;
}

void printObjectNameValidationError(std::ostream& os,
    ObjectNameValidationResult error,
    std::string_view prefix,
    std::string_view variableName)
{
    os << prefix;

    switch (error)
    {
    case ObjectNameValidationResult::Success:
        // This shouldn't be called with Success, but handle it gracefully
        os << variableName << " is valid\n";
        break;

    case ObjectNameValidationResult::Empty:
        os << variableName << " is empty\n";
        break;

    case ObjectNameValidationResult::TooLong_Name:
        os << variableName << " is longer than limit of: "
            << MAX_OBJECT_NAME_SIZE << "\n";
        break;

    case ObjectNameValidationResult::NonAscii:
        os << variableName << " contains non-ASCII characters\n";
        break;

    case ObjectNameValidationResult::ContainsColon:
        os << variableName << " contains colon\n";
        break;

    case ObjectNameValidationResult::TooLong_Id:
        os << variableName << " is longer than limit of: "
            << (MAX_OBJECT_NAME_SIZE * 2 + 1) << "\n";
        break;

    case ObjectNameValidationResult::MissingColon:
        os << variableName << " must contain exactly one colon separator\n";
        break;

    case ObjectNameValidationResult::ContainsMoreThanOneColon:
        os << variableName << " contains more than one colon\n";
        break;

    case ObjectNameValidationResult::PackNameIsEmpty:
        os << variableName << " pack name part (before colon) is empty\n";
        break;

    case ObjectNameValidationResult::PackNameIsTooLong:
        os << variableName << " pack name part is longer than limit of: "
            << MAX_OBJECT_NAME_SIZE << "\n";
        break;

    case ObjectNameValidationResult::ObjectNameisEmpty:
        os << variableName << " object name part (after colon) is empty\n";
        break;

    case ObjectNameValidationResult::ObjectNameIsTooLong:
        os << variableName << " object name part is longer than limit of: "
            << MAX_OBJECT_NAME_SIZE << "\n";
        break;
    }
}


void AssetRegistry::reset()
{
	blockAssetStorage.clear();

	blockDataStorage.clear();
	blockModelDataStorage.clear();

	blockIndexer.clear();
	blockModelIndexer.clear();
}

void AssetRegistry::registerBlock(const BlockAsset& asset)
{
	// Index
	if (blockIndexer.isRegistered(asset.blockStringId))
	{
		std::cerr << "[AssetRegistry]: Block " << asset.blockStringId << " was already registered\n";
		return;
	}
	blockIndexer.registerAndGetId(asset.blockStringId);
	
	// Store asset
	blockAssetStorage.push_back(asset);

	// Create data object
	BlockData& data = blockDataStorage.emplace_back();

	// Properties
	data.blockStringId = asset.blockStringId;
	data.absorbsLight = asset.absorbsLight;
	data.lightEmission = asset.lightEmission;
	data.raycastable = asset.raycastable;

	// Visuals
	for (const auto& assetTexture : asset.textureInfo)
	{
		auto& dataTexture = data.textureSlots.emplace_back();
		dataTexture.textureId = blockTextureIndexer.registerAndGetId(assetTexture.textureName);
		dataTexture.transformation = static_cast<BlockData::TextureSlot::TextureTransformation>(assetTexture.transformation);
		dataTexture.isTranslucent = assetTexture.isTranslucent;
	}

	// Sounds
	data.breakSounds = asset.breakSounds;
	data.placeSounds = asset.placeSounds;
	data.stepSounds = asset.stepSounds;
}

void AssetRegistry::registerBlockModel(const BlockModelData& asset, const std::string& modelName)
{
	// Index
	if (blockModelIndexer.isRegistered(modelName))
	{
		std::cerr << "[AssetRegistry]: Block model " << modelName << " was already registered\n";
		return;
	}
	blockModelIndexer.registerAndGetId(modelName);

	// Store data
	blockModelDataStorage.push_back(asset);
}

bool AssetRegistry::linkAssets()
{
	// Check
	if (blockAssetStorage.size() != blockDataStorage.size())
	{
		std::cerr << "[AssetRegistry]: Asset count does not match data count";
		return false;
	}

	// Get fallback ids
	{
		auto result = blockIndexer.getId("core:air");
		if (result.has_value())
		{
			FALLBACK_BLOCK_ID = result.value();
		}
		else
		{
			std::cerr << "[AssetRegistry]: core:air block is not found\n";
			return false;
		}
	}
	FALLBACK_BLOCK_MODEL_ID = -1;

	// Link
	if (!linkBlockAssets())
	{
		return false;
	}

	// End
	blockAssetStorage.clear();
	return true;
}

bool AssetRegistry::linkBlockAssets()
{
	const size_t blockCount = blockAssetStorage.size();
	auto& sndMgr = SoundManager::getInstance();
	for (size_t i = 0; i < blockCount; i++)
	{
		const auto& asset = blockAssetStorage[i];
		auto& data = blockDataStorage[i];

		// Model
		if (asset.modelName == "None")
		{
			data.modelId = FALLBACK_BLOCK_MODEL_ID;
		}
		else
		{
			auto blockModelIdResult = blockModelIndexer.getId(asset.modelName);
			if (blockModelIdResult.has_value())
			{
				data.modelId = static_cast<BlockModelId>(blockModelIdResult.value());
			}
			else
			{
				std::cerr << "[AssetRegistry]: Block model " << asset.modelName << " is not registered\n";
				data.modelId = FALLBACK_BLOCK_MODEL_ID;
			}
		}

		// Has faces
		data.hasFaces = !data.textureSlots.empty();

		// Enable culling for faces that have opaque aligned faces in the model
		for (int i = 0; i < 6; i++)
		{
			data.faceCulling[i] = false;
		}
		if (data.hasFaces)
		{
			const auto* model = getBlockModelData(data.modelId);
			if (model)
			{
				for (const auto& alignedFace : model->alignedFaces)
				{
					if (alignedFace.normal >= 6)
					{
						continue;
					}

					bool shouldCull = true;

					if (alignedFace.textureSlot < data.textureSlots.size())
					{
						const auto& textureSlot = data.textureSlots[alignedFace.textureSlot];
						if (textureSlot.isTranslucent)
						{
							shouldCull = false;
						}
					}

					data.faceCulling[alignedFace.normal] = shouldCull;
				}
			}
		}

		// Load sounds
		for (const auto& sound : data.breakSounds)
		{
			sndMgr.loadOgg("block/break/" + sound, "res/Sounds/Blocks/Break/" + sound + ".ogg");
		}
		for (const auto& sound : data.placeSounds)
		{
			sndMgr.loadOgg("block/place/" + sound, "res/Sounds/Blocks/Place/" + sound + ".ogg");
		}
		for (const auto& sound : data.stepSounds)
		{
			sndMgr.loadOgg("block/step/" + sound, "res/Sounds/Blocks/Step/" + sound + ".ogg");
		}
	}

	return true;
}

BlockId AssetRegistry::getBlockNumericalId(const std::string& stringId)
{
	auto result = blockIndexer.getId(stringId);
	return result.has_value() ? static_cast<BlockId>(result.value()) : FALLBACK_BLOCK_ID;
}

BlockModelId AssetRegistry::getBlockModelNumericalId(const std::string& stringId)
{
	auto result = blockModelIndexer.getId(stringId);
	return result.has_value() ? static_cast<BlockModelId>(result.value()) : FALLBACK_BLOCK_MODEL_ID;
}

const BlockData* AssetRegistry::getBlockDataSafe(BlockId numericalId)
{
	return numericalId < blockDataStorage.size() ? &blockDataStorage[numericalId] : &blockDataStorage[FALLBACK_BLOCK_ID];
}

const BlockData* AssetRegistry::getBlockData(BlockId numericalId)
{
	return numericalId < blockDataStorage.size() ? &blockDataStorage[numericalId] : nullptr;
}

const BlockModelData* AssetRegistry::getBlockModelData(BlockModelId numericalId)
{
	return numericalId < blockModelDataStorage.size() ? &blockModelDataStorage[numericalId] : nullptr;
}

std::vector<std::string> AssetRegistry::getTextureNames()
{
	const auto& map = blockTextureIndexer.getNameToIDMap();

	// Copy pairs to vector and sort
	std::vector<std::pair<std::string, size_t>> sortedPairs(
		map.begin(), map.end()
	);

	std::sort(sortedPairs.begin(), sortedPairs.end(),
		[](const auto& a, const auto& b) {
			return a.second < b.second;
		});

	// Create result vector
	std::vector<std::string> names;
	names.reserve(names.size());

	for (const auto& item : sortedPairs)
	{
		names.push_back(item.first);
	}

	return names;
}
