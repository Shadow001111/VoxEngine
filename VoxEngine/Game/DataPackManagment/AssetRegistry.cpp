#include "AssetRegistry.h"
#include <iostream>
#include <algorithm>
#include "Game/SoundManager.h"

DynamicArray<BlockAsset> AssetRegistry::blockAssetStorage;
DynamicArray<ItemAsset> AssetRegistry::itemAssetStorage;

DynamicArray<BlockData> AssetRegistry::blockDataStorage;
DynamicArray<BlockModelData> AssetRegistry::blockModelDataStorage;
DynamicArray<ItemData> AssetRegistry::itemDataStorage;
DynamicArray<ItemModelData> AssetRegistry::itemModelDataStorage;

StringIndexer AssetRegistry::blockIndexer;
StringIndexer AssetRegistry::blockModelIndexer;
StringIndexer AssetRegistry::itemIndexer;
StringIndexer AssetRegistry::itemModelIndexer;

StringIndexer AssetRegistry::blockTextureIndexer;
StringIndexer AssetRegistry::itemUITextureIndexer;

BlockId AssetRegistry::FALLBACK_BLOCK_ID;
ModelId AssetRegistry::FALLBACK_BLOCK_MODEL_ID;
ItemId AssetRegistry::FALLBACK_ITEM_ID;
ModelId AssetRegistry::FALLBACK_ITEM_MODEL_ID;

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
	itemAssetStorage.clear();

	blockDataStorage.clear();
	blockModelDataStorage.clear();
	itemDataStorage.clear();
	itemModelDataStorage.clear();

	blockIndexer.clear();
	blockModelIndexer.clear();
	itemIndexer.clear();
	itemModelIndexer.clear();

	blockTextureIndexer.clear();
	itemUITextureIndexer.clear();
}

void AssetRegistry::registerBlock(const BlockAsset& asset)
{
	// Index
	if (blockIndexer.isRegistered(asset.stringId))
	{
		std::cerr << "[AssetRegistry]: Block " << asset.stringId << " was already registered\n";
		return;
	}
	blockIndexer.registerAndGetId(asset.stringId);
	
	// Store asset
	blockAssetStorage.push_back(asset);

	// Create data object
	BlockData& data = blockDataStorage.emplace_back();

	// Properties
	data.stringId = asset.stringId;
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

void AssetRegistry::registerBlockModel(const BlockModelData& asset, const std::string& modelStringId)
{
	// Index
	if (blockModelIndexer.isRegistered(modelStringId))
	{
		std::cerr << "[AssetRegistry]: Block model " << modelStringId << " is already registered\n";
		return;
	}
	blockModelIndexer.registerAndGetId(modelStringId);

	// Store data
	blockModelDataStorage.push_back(asset);
}

void AssetRegistry::registerItem(const ItemAsset& asset)
{
	// Index
	if (itemIndexer.isRegistered(asset.stringId))
	{
		std::cerr << "[AssetRegistry]: Item " << asset.stringId << " is already registered\n";
		return;
	}
	itemIndexer.registerAndGetId(asset.stringId);

	// Store asset
	itemAssetStorage.push_back(asset);

	// Create data object
	ItemData& data = itemDataStorage.emplace_back();

	// Copy data
	data.stringId = asset.stringId; // TOOD: May move
	data.stackSize = asset.stackSize;
	data.hasBlockPlaceable = asset.hasBlockPlaceable;
	data.uiTextureId = itemUITextureIndexer.registerAndGetId(asset.uiTextureName);
}

void AssetRegistry::registerItemModel(const ItemModelData& asset, const std::string& modelStringId)
{
	// Index
	if (itemModelIndexer.isRegistered(modelStringId))
	{
		std::cerr << "[AssetRegistry]: Item model " << modelStringId << " is already registered\n";
		return;
	}
	itemModelIndexer.registerAndGetId(modelStringId);

	// Store data
	itemModelDataStorage.push_back(asset);
}

bool AssetRegistry::linkAssets()
{
	// Check
	if (
		blockAssetStorage.size() != blockDataStorage.size() ||
		itemAssetStorage.size() != itemDataStorage.size()
		)
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
	FALLBACK_ITEM_ID = -1;
	FALLBACK_ITEM_MODEL_ID = -1;

	// Link
	if (
		!linkBlockAssets() ||
		!linkItemAssets()
		)
	{
		return false;
	}

	// End
	blockAssetStorage.clear(); // TODO: Destroy them
	itemAssetStorage.clear();
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
				data.modelId = static_cast<ModelId>(blockModelIdResult.value());
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

bool AssetRegistry::linkItemAssets()
{
	const size_t itemCount = itemAssetStorage.size();
	for (size_t i = 0; i < itemCount; i++)
	{
		const auto& asset = itemAssetStorage[i];
		auto& data = itemDataStorage[i];

		// Block placeable
		if (data.hasBlockPlaceable)
		{
			auto blockIdResult = blockIndexer.getId(asset.blockPlaceableStringId);
			if (blockIdResult.has_value())
			{
				data.blockPlaceableId = static_cast<BlockId>(blockIdResult.value());
			}
			else
			{
				std::cerr << "[AssetRegistry]: Block " << asset.blockPlaceableStringId << " is not registered\n";
				data.hasBlockPlaceable = false;
			}
		}
	}
	return true;
}

BlockId AssetRegistry::getBlockNumericalId(const std::string& stringId)
{
	auto result = blockIndexer.getId(stringId);
	return result.has_value() ? static_cast<BlockId>(result.value()) : FALLBACK_BLOCK_ID;
}

//ModelId AssetRegistry::getBlockModelNumericalId(const std::string& stringId)
//{
//	auto result = blockModelIndexer.getId(stringId);
//	return result.has_value() ? static_cast<ModelId>(result.value()) : FALLBACK_BLOCK_MODEL_ID;
//}

ItemId AssetRegistry::getItemNumericalId(const std::string& stringId)
{
	auto result = itemIndexer.getId(stringId);
	return result.has_value() ? static_cast<ItemId>(result.value()) : FALLBACK_ITEM_ID;
}

const BlockData* AssetRegistry::getBlockDataSafe(BlockId numericalId)
{
	return numericalId < blockDataStorage.size() ? &blockDataStorage[numericalId] : &blockDataStorage[FALLBACK_BLOCK_ID];
}

const BlockData* AssetRegistry::getBlockData(BlockId numericalId)
{
	return numericalId < blockDataStorage.size() ? &blockDataStorage[numericalId] : nullptr;
}

const BlockModelData* AssetRegistry::getBlockModelData(ModelId numericalId)
{
	// TODO: Use something faster than vector.size()
	return numericalId < blockModelDataStorage.size() ? &blockModelDataStorage[numericalId] : nullptr;
}

const ItemData* AssetRegistry::getItemDataSafe(ItemId numericalId)
{
	return numericalId < itemDataStorage.size() ? &itemDataStorage[numericalId] : &itemDataStorage[FALLBACK_ITEM_ID];
}

const ItemData* AssetRegistry::getItemData(ItemId numericalId)
{
	return numericalId < itemDataStorage.size() ? &itemDataStorage[numericalId] : nullptr;
}

const ItemModelData* AssetRegistry::getItemModelData(ModelId numericalId)
{
	return numericalId < itemModelDataStorage.size() ? &itemModelDataStorage[numericalId] : nullptr;
}

std::vector<std::string> AssetRegistry::sortMapAndReturnNames(const robin_hood::unordered_flat_map<std::string, size_t>& map)
{
	// Copy pairs to vector and sort
	std::vector<robin_hood::pair<std::string, size_t>> sortedPairs(
		map.begin(), map.end()
	);

	std::sort(sortedPairs.begin(), sortedPairs.end(),
		[](const auto& a, const auto& b) {
			return a.second < b.second;
		});

	// Create result vector
	std::vector<std::string> names;
	names.reserve(sortedPairs.size());
	for (const auto& item : sortedPairs)
	{
		names.emplace_back(std::move(item.first));
	}

	return names;
}

std::vector<std::string> AssetRegistry::getBlockTextureNames()
{
	return sortMapAndReturnNames(blockTextureIndexer.getNameToIDMap());
}

std::vector<std::string> AssetRegistry::getItemUITextureNames()
{
	return sortMapAndReturnNames(itemUITextureIndexer.getNameToIDMap());
}
