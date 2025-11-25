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

struct BlockTextures
{
	uint16_t textureIDs[6] = { 0, 0, 0, 0, 0, 0 };
	uint16_t texturesTransformation = 0;

	BlockTextures() = default;
	BlockTextures(uint16_t texturesTransformation);
};

struct BlockTextureNames
{
	const char* texName_negativeX = nullptr;
	const char* texName_positiveX = nullptr;
	const char* texName_negativeY = nullptr;
	const char* texName_positiveY = nullptr;
	const char* texName_negativeZ = nullptr;
	const char* texName_positiveZ = nullptr;

	BlockTextureNames() = default;
	BlockTextureNames(
		const char* nxName, const char* pxName,
		const char* nyName, const char* pyName,
		const char* nzName, const char* pzName
	);
};

struct BlockData
{
	BlockProperties properties;
	BlockTextures textures;

	BlockData() = default;
	BlockData(const BlockProperties& properties, const BlockTextures& texturs);

	//BlockData(const BlockData& other) = delete;
	//BlockData& operator=(const BlockData& other) = delete;
};

class BlockRegistry
{
	BlockRegistry() = delete;
	~BlockRegistry() = delete;

	// TODO: Maybe change to vectors
	static std::unordered_map<BlockID, BlockData> BLOCK_DATABASE;
	static std::unordered_map<BlockID, BlockTextureNames> TEXTURE_NAMES;
	static StringIndexer blockIndexer;

	static void registerBlock(const std::string& blockName,
		const BlockProperties& properties,
		const BlockTextureNames& textureNames,
		uint16_t texturesTransformation);
public:
	static void registerBlocks(std::vector<std::string>& textureNames);
private:
	static void buildTextureIDs(std::vector<std::string>& textureNames);
public:
	// Retrieve block ID from block name (returns SIZE_MAX if not found)
	static BlockID getBlockID(const std::string& blockName);

	static const BlockData* getBlockDataByName(const std::string& blockName);
	static const BlockData* getBlockDataByID(BlockID id);
};