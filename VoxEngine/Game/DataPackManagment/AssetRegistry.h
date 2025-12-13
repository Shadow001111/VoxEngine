#pragma once
#include "AssetTypes/BlockAsset.h"
#include "DataTypes/BlockModelData.h"

#include "DataTypes/BlockData.h"
//#include "DataTypes/BlockModelData.h"

#include "Core/StringIndexer.h"

constexpr size_t MAX_OBJECT_NAME_SIZE = 64;
constexpr size_t MAX_BLOCK_TEXTURE_SLOT_COUNT = 64;
constexpr size_t MAX_BLOCK_MODEL_FACE_COUNT = 64;

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
	static std::vector<BlockAsset> blockAssetStorage;

	// Data objects are used directly in game
	static std::vector<BlockData> blockDataStorage;
	static std::vector<BlockModelData> blockModelDataStorage;

	// Indexers
	static StringIndexer blockIndexer;
	static StringIndexer blockModelIndexer;
	static StringIndexer blockTextureIndexer;
	
	// Fallback ids
	static BlockId FALLBACK_BLOCK_ID;
	static BlockModelId FALLBACK_BLOCK_MODEL_ID;
public:
	//
	static void reset();

	// Registration methods
	static void registerBlock(const BlockAsset& asset);
	static void registerBlockModel(const BlockModelData& asset, const std::string& modelName);

	//
	static bool linkAssets();
private:
	static bool linkBlockAssets();
public:
	// Data id getters
	static BlockId getBlockNumericalId(const std::string& stringId);
	static BlockModelId getBlockModelNumericalId(const std::string& stringId);

	// Always returns valid pointer - never nullptr
	static const BlockData* getBlockDataSafe(BlockId numericalId);
	// Returns nullptr if ID is out of bounds
	static const BlockData* getBlockData(BlockId numericalId);

	static const BlockModelData* getBlockModelData(BlockModelId numericalId);

	//
	static std::vector<std::string> getTextureNames();
};

