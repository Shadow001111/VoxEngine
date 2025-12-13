#pragma once
#include <filesystem>
#include <string>
#include <optional>
#include <json.hpp>

#include "AssetTypes/BlockAsset.h"
#include "DataTypes/BlockModelData.h"

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
	static std::optional<BlockModelData::AlignedFace> parseBlockModelAlignedFacesJson(const json& j);
	static std::optional<BlockModelData::NonAlignedFace> parseBlockModelNonAlignedFacesJson(const json& j);
};

