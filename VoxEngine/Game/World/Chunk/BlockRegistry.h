#pragma once
#include "Block.h"
#include "BlockModelLoader.h"

#include <vector>
#include <string>
#include <unordered_map>

#include "Core/StringIndexer.h"

struct BlockProperties
{
	bool absorbsLight = false;
	uint8_t lightEmission = 0;
	bool hasFaces = false;
	bool faceCulling[6] = { false, false, false, false, false, false };
	bool raycastable = false;

	BlockProperties() = default;
	BlockProperties(bool absorbsLight, uint8_t lightEmission, bool raycastable);
};

struct TextureInfo
{
	enum class TextureTransformation : uint8_t
	{
		None = 0,
		Flip = 1,
		RotateAndFlip = 2
	};

	std::string name;
	TextureTransformation transformation = TextureTransformation::None;
	bool isTranslucent = false;

	TextureInfo() = default;
	TextureInfo(const std::string& name, TextureTransformation transformation, bool isTranslucent);
};

struct TextureSlot
{
	uint32_t textureID = 0;
	TextureInfo::TextureTransformation transformation = TextureInfo::TextureTransformation::None;
	bool isTranslucent = false;

	TextureSlot() = default;
	TextureSlot(uint32_t textureID, TextureInfo::TextureTransformation transformation, bool isTranslucent);
};

struct BlockVisuals
{
	uint32_t modelID = 0;
	std::vector<TextureSlot> textureSlots;

	BlockVisuals() = default;
};

// TODO: Shouldn't be visible to other files
struct BlockTempInfo
{
	std::string modelName;

	std::vector<TextureInfo> textureInfo;

	BlockTempInfo() = default;
	BlockTempInfo(
		const char* modelName,
		const std::vector<TextureInfo>& textureSlots
	);
};

// Maybe use sound IDs instead of names?
struct BlockSounds
{
	std::vector<std::string> breakSounds;
	std::vector<std::string> placeSounds;
	std::vector<std::string> stepSounds;

	BlockSounds() = default;
	BlockSounds(
		const std::vector<std::string>& breakSounds,
		const std::vector<std::string>& placeSounds,
		const std::vector<std::string>& stepSounds
	);
};

struct BlockData
{
	BlockProperties properties;
	BlockVisuals visuals;
	BlockSounds sounds;

	BlockData() = default;
	BlockData(const BlockProperties& properties, const BlockVisuals& visuals, const BlockSounds& sounds);
};

class BlockRegistry
{
	BlockRegistry() = delete;
	~BlockRegistry() = delete;

	static std::unordered_map<BlockID, BlockData> blockDataStorage;
	static std::unordered_map<BlockID, BlockTempInfo> blockTempInfoStorage;
	static std::unordered_map<size_t, BlockModelLoader::BlockModel> blockModelStorage;
	static StringIndexer blockIndexer;

	static void registerBlock(
		const std::string& blockName,
		const BlockProperties& properties,
		const BlockSounds& sounds,
		const BlockTempInfo& tempInfo
	);

	static void registerBlocksFromFile(const std::filesystem::path& filepath);
	static void registerBlocksFromJson(const json& j);
	static BlockProperties parseJsonBlockProperties(const json& j);
	static BlockTempInfo parseJsonBlockTempInfo(const json& j);
	static BlockSounds parseJsonBlockSounds(const json& j);
public:
	static void registerBlocks(std::vector<std::string>& textureNames);
private:
	static void postBuild(std::vector<std::string>& textureNames);
	static void loadBlockSounds();
public:
	// Retrieve block ID from block name (returns SIZE_MAX if not found)
	static BlockID getBlockID(const std::string& blockName);

	static const BlockData* getBlockDataByName(const std::string& blockName);
	static const BlockData* getBlockDataByID(BlockID id);
	static const BlockModelLoader::BlockModel* getBlockModelByID(size_t modelID);
};