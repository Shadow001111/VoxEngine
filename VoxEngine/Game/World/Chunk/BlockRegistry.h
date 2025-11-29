#pragma once
#include "Block.h"

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

enum class TextureTransformation : uint8_t
{
	None = 0,
	Flip = 1,
	RotateAndFlip = 2
};

struct TextureInfo
{
	std::string name;
	TextureTransformation transformation = TextureTransformation::None;
	bool isTranslucent = false;

	TextureInfo() = default;
	TextureInfo(const std::string& name, TextureTransformation transformation, bool isTranslucent);
};

struct TextureSlot
{
	uint32_t textureID = 0;
	TextureTransformation transformation = TextureTransformation::None;
	bool isTranslucent = false;

	TextureSlot() = default;
	TextureSlot(uint32_t textureID, TextureTransformation transformation, bool isTranslucent);
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

struct BlockData
{
	BlockProperties properties;
	BlockVisuals visuals;

	BlockData() = default;
	BlockData(const BlockProperties& properties, const BlockVisuals& textures);

	//BlockData(const BlockData& other) = delete;
	//BlockData& operator=(const BlockData& other) = delete;
};

class BlockRegistry
{
	BlockRegistry() = delete;
	~BlockRegistry() = delete;

	// TODO: Maybe change to vectors. Then change in BlockModelLoader.
	static std::unordered_map<BlockID, BlockData> blockDataStorage;
	static std::unordered_map<BlockID, BlockTempInfo> blockTempInfoStorage;
	static StringIndexer blockIndexer;

	static void registerBlock(
		const std::string& blockName,
		const BlockProperties& properties,
		const BlockTempInfo& tempInfo
	);
public:
	static void registerBlocks(
		std::vector<std::string>& textureNames
	);
private:
	static void buildIDs(
		std::vector<std::string>& textureNames,
		std::vector<std::string>& modelNames
	);
	static void updateBlockPropertiesBasedOnModels();
public:
	// Retrieve block ID from block name (returns SIZE_MAX if not found)
	static BlockID getBlockID(const std::string& blockName);

	static const BlockData* getBlockDataByName(const std::string& blockName);
	static const BlockData* getBlockDataByID(BlockID id);
};