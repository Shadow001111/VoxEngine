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
	bool areFacesTransparent = false; // Can be array of 6
	bool raycastable = false;

	BlockProperties() = default;
	BlockProperties(bool absorbsLight, uint8_t lightEmission, bool hasFaces, bool areFacesTransparent, bool raycastable);
};

enum class TextureTransformation : uint8_t
{
	None = 0,
	Flip = 1,
	RotateAndFlip = 2
};

struct BlockVisuals
{
	uint32_t modelID = 0;
	std::vector<std::pair<uint32_t, TextureTransformation>> textureSlots;

	BlockVisuals() = default;
};

// TODO: Shouldn't be visible to other files
struct BlockTempInfo
{
	std::string modelName;
	std::vector<std::pair<std::string, TextureTransformation>> textureInfo;

	BlockTempInfo() = default;
	BlockTempInfo(
		const char* modelName,
		const std::vector<std::pair<std::string, TextureTransformation>>& textureSlots
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
		std::vector<std::string>& textureNames,
		std::vector<std::string>& modelNames
	);
private:
	static void buildIDs(
		std::vector<std::string>& textureNames,
		std::vector<std::string>& modelNames
	);
public:
	// Retrieve block ID from block name (returns SIZE_MAX if not found)
	static BlockID getBlockID(const std::string& blockName);

	static const BlockData* getBlockDataByName(const std::string& blockName);
	static const BlockData* getBlockDataByID(BlockID id);
};