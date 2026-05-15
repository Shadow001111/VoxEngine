#include "AssetRegistry.h"

#include "Game/TracyProfiler.h"
#include "Game/ProfileCategories.h"

//#include "AudioEngine/AudioEngine.h"

#include <iostream>
#include <algorithm>

DynamicArray<BlockAsset> AssetRegistry::blockAssetStorage;
DynamicArray<ItemAsset> AssetRegistry::itemAssetStorage;

DynamicArray<BlockDataHot> AssetRegistry::blockDataHotStorage;
DynamicArray<BlockDataCold> AssetRegistry::blockDataColdStorage;
DynamicArray<BlockModelData> AssetRegistry::blockModelDataStorage;
DynamicArray<ItemData> AssetRegistry::itemDataStorage;
DynamicArray<ItemModelData> AssetRegistry::itemModelDataStorage;

StringIndexer AssetRegistry::blockIndexer;
StringIndexer AssetRegistry::blockModelIndexer;
StringIndexer AssetRegistry::itemIndexer;
StringIndexer AssetRegistry::itemModelIndexer;

StringIndexer AssetRegistry::blockTextureIndexer;
StringIndexer AssetRegistry::itemUITextureIndexer;

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

template <class SoundSet>
static void loadUniqueSounds(
	const SoundSet& sounds,
	std::string_view logicalPrefix,
	std::string_view diskPrefix)
{
	//auto& sndMgr = SoundManager::getInstance();
	//
	//TRACY_SCOPE_NC("Load sounds", ProfileCategory::General);
	//
	//std::string logicalName;
	//std::string diskName;
	//
	//for (const auto& sound : sounds)
	//{
	//	logicalName.clear();
	//	logicalName.reserve(logicalPrefix.size() + sound.size());
	//	logicalName += logicalPrefix;
	//	logicalName += sound;
	//
	//	diskName.clear();
	//	diskName.reserve(diskPrefix.size() + sound.size() + 4);
	//	diskName += diskPrefix;
	//	diskName += sound;
	//	diskName += ".ogg";
	//
	//	sndMgr.loadOgg(logicalName, diskName);
	//}
}


void AssetRegistry::reset()
{
	blockAssetStorage.clear();
	itemAssetStorage.clear();

	blockDataHotStorage.clear();
	blockDataColdStorage.clear();
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
	BlockDataHot& dataHot = blockDataHotStorage.emplace_back();
	BlockDataCold& dataCold = blockDataColdStorage.emplace_back();

	// Properties
	dataCold.stringId = asset.stringId;
	dataHot.lightAbsorbing = asset.lightAbsorbing;
	dataHot.lightEmission = asset.lightEmission;
	dataHot.raycastable = asset.raycastable;

	// Visuals
	dataHot.textureSlots.reserve(asset.textureInfo.size());
	for (const auto& assetTexture : asset.textureInfo)
	{
		auto& dataTexture = dataHot.textureSlots.emplace_back();
		dataTexture.textureId = blockTextureIndexer.registerAndGetId(assetTexture.textureName);
		dataTexture.transformation = static_cast<BlockDataHot::TextureSlot::TextureTransformation>(assetTexture.transformation);
		dataTexture.isTranslucent = assetTexture.isTranslucent;
	}

	// Sounds
	dataCold.breakSounds = asset.breakSounds;
	dataCold.placeSounds = asset.placeSounds;
	dataCold.stepSounds = asset.stepSounds;
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
		blockAssetStorage.size() != blockDataHotStorage.size() ||
		blockAssetStorage.size() != blockDataColdStorage.size() ||
		itemAssetStorage.size() != itemDataStorage.size()
		)
	{
		std::cerr << "[AssetRegistry]: Asset count does not match data count";
		return false;
	}

	if (!ensureAirIdIs0())
	{
		return false;
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
		std::cerr << "[AssetRegistry]: Failed to link assets";
		return false;
	}

	// End
	blockAssetStorage.clear();
	blockAssetStorage.shrink_to_fit();

	itemAssetStorage.clear();
	itemAssetStorage.shrink_to_fit();
	return true;
}

bool AssetRegistry::linkBlockAssets()
{
	const size_t blockCount = blockAssetStorage.size();

	robin_hood::unordered_flat_set<std::string> breakSounds;
	robin_hood::unordered_flat_set<std::string> placeSounds;
	robin_hood::unordered_flat_set<std::string> stepSounds;

	breakSounds.reserve(blockCount);
	placeSounds.reserve(blockCount);
	stepSounds.reserve(blockCount);

	{
		TRACY_SCOPE_NC("Link assets", ProfileCategory::General);

		for (size_t i = 0; i < blockCount; i++)
		{
			const auto& asset = blockAssetStorage[i];
			auto& dataHot = blockDataHotStorage[i];

			// Model
			if (asset.modelName == "None")
			{
				dataHot.modelId = FALLBACK_BLOCK_MODEL_ID;
			}
			else
			{
				auto blockModelIdResult = blockModelIndexer.getId(asset.modelName);
				if (blockModelIdResult.has_value())
				{
					dataHot.modelId = static_cast<ModelId>(blockModelIdResult.value());
				}
				else
				{
					std::cerr << "[AssetRegistry]: Block model " << asset.modelName << " is not registered\n";
					dataHot.modelId = FALLBACK_BLOCK_MODEL_ID;
				}
			}

			// Has faces
			dataHot.hasFaces = !dataHot.textureSlots.empty();

			// Enable culling for faces that have opaque aligned faces in the model
			for (int i = 0; i < 6; i++)
			{
				dataHot.faceCulling.set(i, false);
			}
			if (dataHot.hasFaces)
			{
				const auto* model = getBlockModelData(dataHot.modelId);
				if (model)
				{
					for (const auto& alignedFace : model->alignedFaces)
					{
						if (alignedFace.normal >= 6)
						{
							continue;
						}

						bool shouldCull = true;

						if (alignedFace.textureSlot < dataHot.textureSlots.size())
						{
							const auto& textureSlot = dataHot.textureSlots[alignedFace.textureSlot];
							if (textureSlot.isTranslucent)
							{
								shouldCull = false;
							}
						}

						dataHot.faceCulling.set(alignedFace.normal, shouldCull);
					}
				}
			}
			dataHot.lightAbsorbing &= dataHot.faceCulling;

			// Collect sounds only. Load them later, once each.
			for (const auto& s : asset.breakSounds) breakSounds.emplace(s);
			for (const auto& s : asset.placeSounds) placeSounds.emplace(s);
			for (const auto& s : asset.stepSounds) stepSounds.emplace(s);
		}
	}

	loadUniqueSounds(breakSounds, "block/break/", "res/Sounds/Blocks/Break/");
	loadUniqueSounds(placeSounds, "block/place/", "res/Sounds/Blocks/Place/");
	loadUniqueSounds(stepSounds, "block/step/", "res/Sounds/Blocks/Step/");

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

bool AssetRegistry::ensureAirIdIs0()
{
	// Find current ID of core:air
	auto airIdOpt = blockIndexer.getId("core:air");
	if (!airIdOpt.has_value())
	{
		std::cerr << "[AssetRegistry]: 'core:air' not registered!\n";
		return false;
	}

	BlockId airId = airIdOpt.value();
	if (airId == 0) return true;

	// Swap the indexer mappings so the string IDs match the new positions
	const std::string& firstBlockStringId = blockDataColdStorage[0].stringId;
	if (!blockIndexer.swapIds("core:air", firstBlockStringId))
	{
		std::cerr << "[AssetRegistry]: Failed to swap 'core:air' with other block\n";
		return false;
	}

	// Swap block data entries
	std::swap(blockDataHotStorage[0], blockDataHotStorage[airId]);
	std::swap(blockDataColdStorage[0], blockDataColdStorage[airId]);

	return true;
}

BlockId AssetRegistry::getBlockNumericalId(const std::string& stringId)
{
	auto result = blockIndexer.getId(stringId);
	return result.value_or(FALLBACK_BLOCK_ID);
}

//ModelId AssetRegistry::getBlockModelNumericalId(const std::string& stringId)
//{
//	auto result = blockModelIndexer.getId(stringId);
//	return result.has_value() ? static_cast<ModelId>(result.value()) : FALLBACK_BLOCK_MODEL_ID;
//}

ItemId AssetRegistry::getItemNumericalId(const std::string& stringId)
{
	auto result = itemIndexer.getId(stringId);
	return result.value_or(FALLBACK_ITEM_ID);
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
