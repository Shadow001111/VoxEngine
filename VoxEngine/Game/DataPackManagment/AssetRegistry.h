#pragma once
#include "AssetTypes/BlockAsset.h"
#include "DataTypes/BlockModelData.h"
#include "AssetTypes/ItemAsset.h"
#include "DataTypes/ItemModelData.h"

#include "DataTypes/BlockData.h"
//#include "DataTypes/BlockModelData.h"
#include "DataTypes/ItemData.h"
//#include "DataTypes/ItemModelData.h"

#include "Core/StringIndexer.h"
#include "Core/Container/DynamicArray.h"

constexpr size_t MAX_OBJECT_NAME_SIZE = 64;
constexpr size_t MAX_TEXTURE_SLOT_COUNT = 64;
constexpr size_t MAX_MODEL_FACE_COUNT = 64;

enum class ObjectNameValidationResult
{
	// Name
	Success = 0,
	Empty,
	TooLong_Name,
	NonAscii,
	ContainsColon,

	// StringId special
	TooLong_Id,
	MissingColon,
	ContainsMoreThanOneColon,
	PackNameIsEmpty,
	PackNameIsTooLong,
	ObjectNameisEmpty,
	ObjectNameIsTooLong
};

ObjectNameValidationResult validateObjectName(std::string_view name) noexcept;
ObjectNameValidationResult validateObjectStringId(std::string_view stringId) noexcept;
void printObjectNameValidationError(std::ostream& os,
	ObjectNameValidationResult error,
	std::string_view prefix,
	std::string_view variableName);

class AssetRegistry
{
	// Assets are used for loading and linking data objects
	static DynamicArray<BlockAsset> blockAssetStorage;
	static DynamicArray<ItemAsset> itemAssetStorage;

	// Data objects are used directly in game
	static DynamicArray<BlockData> blockDataStorage;
	static DynamicArray<BlockModelData> blockModelDataStorage;
	static DynamicArray<ItemData> itemDataStorage;
	static DynamicArray<ItemModelData> itemModelDataStorage;

	// Indexers
	static StringIndexer blockIndexer;
	static StringIndexer blockModelIndexer;
	static StringIndexer itemIndexer;
	static StringIndexer itemModelIndexer;

	static StringIndexer blockTextureIndexer;
	static StringIndexer itemUITextureIndexer;
	
	// Fallback ids
	static BlockId FALLBACK_BLOCK_ID;
	static ModelId FALLBACK_BLOCK_MODEL_ID;
	static ItemId FALLBACK_ITEM_ID;
	static ModelId FALLBACK_ITEM_MODEL_ID;
public:
	//
	static void reset();

	// Registration methods
	static void registerBlock(const BlockAsset& asset);
	static void registerBlockModel(const BlockModelData& asset, const std::string& modelStringId);
	static void registerItem(const ItemAsset& asset);
	static void registerItemModel(const ItemModelData& asset, const std::string& modelStringId);

	//
	static bool linkAssets();
private:
	static bool linkBlockAssets();
	static bool linkItemAssets();
public:
	// Data id getters
	static BlockId getBlockNumericalId(const std::string& stringId);
	//static ModelId getBlockModelNumericalId(const std::string& stringId);
	static ItemId getItemNumericalId(const std::string& stringId);

	// Always returns valid pointer - never nullptr
	static const BlockData* getBlockDataSafe(BlockId numericalId);
	// Returns nullptr if ID is out of bounds
	static const BlockData* getBlockData(BlockId numericalId);

	static const BlockModelData* getBlockModelData(ModelId numericalId);

	// Always returns valid pointer - never nullptr
	static const ItemData* getItemDataSafe(ItemId numericalId);
	// Returns nullptr if ID is out of bounds
	static const ItemData* getItemData(ItemId numericalId);

	static const ItemModelData* getItemModelData(ModelId numericalId);

	//
private:
	static std::vector<std::string> sortMapAndReturnNames(const robin_hood::unordered_flat_map<std::string, size_t>& map);
public:
	static std::vector<std::string> getBlockTextureNames();
	static std::vector<std::string> getItemUITextureNames();
};

