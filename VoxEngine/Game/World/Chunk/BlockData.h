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

struct BlockTextureNames
{
	const char* texName_negativeX = nullptr;
	const char* texName_positiveX = nullptr;
	const char* texName_negativeY = nullptr;
	const char* texName_positiveY = nullptr;
	const char* texName_negativeZ = nullptr;
	const char* texName_positiveZ = nullptr;

	BlockTextureNames() = default;
	BlockTextureNames(const char* nxName, const char* pxName, const char* nyName, const char* pyName, const char* nzName, const char* pzName);
};

struct BlockData
{
	BlockProperties properties;
	BlockTextureNames textureNames;

	BlockData() = default;
	BlockData(const BlockProperties& properties, const BlockTextureNames& textureNames);
};

class BlockDataBase
{
	BlockDataBase() = delete;
	~BlockDataBase() = delete;

	static BlockData BLOCK_DATABASE[(size_t)Block::__BlockCount__];

	static void registerBlock(Block block, const BlockProperties& properties, const BlockTextureNames& textureNames);
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
	BlockTextureIDs* blockTexturesIDs;
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