#pragma once
#include "Block.h"

#include <vector>
#include <string>

struct BlockProperties
{
	uint8_t lightAbsorption = 1;
	uint8_t lightEmission = 0;
	bool hasFaces = false;
	bool areFacesTransparent = false; // Can be array of 6
	bool raycastable = false;

	BlockProperties() = default;
	BlockProperties(uint8_t lightAbsorption, uint8_t lightEmission, bool hasFaces, bool areFacesTransparent, bool raycastable);
};

enum class TextureTransformation : uint8_t
{
	None = 0,
	Flip = 1,
	RotateAndFlip = 2
};

struct BlockTextures
{
	const char* texName_negativeX = nullptr;
	const char* texName_positiveX = nullptr;
	const char* texName_negativeY = nullptr;
	const char* texName_positiveY = nullptr;
	const char* texName_negativeZ = nullptr;
	const char* texName_positiveZ = nullptr;

	uint16_t texturesTransformation = 0;

	BlockTextures() = default;
	BlockTextures(
		const char* nxName, const char* pxName,
		const char* nyName, const char* pyName,
		const char* nzName, const char* pzName,
		TextureTransformation nxTransform, TextureTransformation pxTransform,
		TextureTransformation nyTransform, TextureTransformation pyTransform,
		TextureTransformation nzTransform, TextureTransformation pzTransform
	);
};

// TODO: Add block texture IDs there
struct BlockData
{
	BlockProperties properties;
	BlockTextures textures;

	BlockData() = default;
	BlockData(const BlockProperties& properties, const BlockTextures& texturs);
};

class BlockDataBase
{
	BlockDataBase() = delete;
	~BlockDataBase() = delete;

	static BlockData BLOCK_DATABASE[(size_t)Block::__BlockCount__];

	static void registerBlock(Block block, const BlockProperties& properties, const BlockTextures& textures);
public:
	static void loadBlockDataBase();

	static const BlockData* getBlockData(Block block);
	static const BlockData* getBlockData(size_t index);
};

class BlockTextureIDDatabase
{
	struct BlockTextureIDs
	{
		uint16_t ids[6];
	};
	std::vector<BlockTextureIDs> blockTexturesIDs;
public:
	BlockTextureIDDatabase();
	~BlockTextureIDDatabase();

	void build(std::vector<std::string>& textureNames);

	BlockTextureIDDatabase(const BlockTextureIDDatabase& other) = delete;
	BlockTextureIDDatabase& operator=(const BlockTextureIDDatabase& other) = delete;
	BlockTextureIDDatabase(BlockTextureIDDatabase&& other) = delete;
	BlockTextureIDDatabase& operator=(BlockTextureIDDatabase&& other) = delete;

	const BlockTextureIDs& getBlockTextureIDs(Block block) const;
};