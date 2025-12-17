#pragma once
#include <filesystem>
#include <string>
#include <optional>
#include <json.hpp>

#include "AssetTypes/BlockAsset.h"
#include "DataTypes/BlockModelData.h"
#include "AssetTypes/ItemAsset.h"
#include "DataTypes/ItemModelData.h"

using json = nlohmann::json;

class DataPackManager
{
	struct DatapackMetadata
	{
		std::string name;
		std::string id;
	};
public:
	static void loadAllDataPacks();
private:
	static void loadDataPack(const std::filesystem::path& dataPackPath);

	static std::optional<DatapackMetadata> getDatapackMetadata(const std::filesystem::path& dataPackPath);
	static std::optional<DatapackMetadata> parseMetadataJson(const json& j);

	static void loadBlocks(const std::filesystem::path& dataPackPath, const std::string& dataPackStringId);
	static bool parseBlockJson(const json& j, BlockAsset& outAsset);
	static bool parseBlockPropertiesJson(const json& j, BlockAsset& outAsset);
	static bool parseBlockVisualsJson(const json& j, BlockAsset& outAsset);
	static bool parseBlockSoundsJson(const json& j, BlockAsset& outAsset);

	static void loadBlockModels(const std::filesystem::path& dataPackPath, const std::string& dataPackStringId);
	static bool parseBlockModelJson(const json& j, BlockModelData& outAsset);
	static std::optional<BlockModelData::AlignedFace> parseBlockModelAlignedFaceJson(const json& j);
	static std::optional<BlockModelData::NonAlignedFace> parseBlockModelNonAlignedFaceJson(const json& j);

	static void loadItems(const std::filesystem::path& dataPackPath, const std::string& dataPackStringId);
	static bool parseItemJson(const json& j, ItemAsset& outAsset);

	static void loadItemModels(const std::filesystem::path& dataPackPath, const std::string& dataPackStringId);
	static bool parseItemModelJson(const json& j, ItemModelData& outAsset);
	static std::optional<ItemModelData::Face> parseItemModelFacesJson(const json& j);
};

