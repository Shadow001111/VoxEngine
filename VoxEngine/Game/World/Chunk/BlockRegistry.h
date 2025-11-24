#pragma once
#include "Block.h"

#include <vector>
#include <string>

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

	BlockData(const BlockData& other) = delete;
	//BlockData& operator=(const BlockData& other) = delete;
};

class BlockDataBase
{
	BlockDataBase() = delete;
	~BlockDataBase() = delete;

	static BlockData BLOCK_DATABASE[(size_t)Block::__BlockCount__];
	static BlockTextureNames TEXTURE_NAMES[(size_t)Block::__BlockCount__];

	static void registerBlock(Block block,
		const BlockProperties& properties,
		const BlockTextureNames& textureNames,
		uint16_t texturesTransformation);
public:
	static void loadBlockDataBase(std::vector<std::string>& textureNames);
private:
	static void buildTextureIDs(std::vector<std::string>& textureNames);
public:

	static const BlockData* getBlockData(Block block);
	static const BlockData* getBlockData(size_t index);
};

#define GET_BLOCK_DATA(block) (BlockDataBase::getBlockData(block))
#define GET_BLOCK_PROPERTIES(block) (BlockDataBase::getBlockData(block)->properties)
#define GET_BLOCK_TEXTURES(block) (BlockDataBase::getBlockData(block)->textures)