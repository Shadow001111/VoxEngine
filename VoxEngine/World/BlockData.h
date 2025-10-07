#pragma once
#include "Block.h"

#include <vector>
#include <string>

struct BlockData
{
	const char* texName_negativeX = nullptr;
	const char* texName_positiveX = nullptr;
	const char* texName_negativeY = nullptr;
	const char* texName_positiveY = nullptr;
	const char* texName_negativeZ = nullptr;
	const char* texName_positiveZ = nullptr;

	BlockData(const char* nxName, const char* pxName, const char* nyName, const char* pyName, const char* nzName, const char* pzName);
};

class BlockDataBase
{
	BlockDataBase() = delete;
	~BlockDataBase() = delete;

	static BlockData BLOCK_DATABASE[(size_t)Block::__BlockCount__];
public:
	static const BlockData& getBlockData(Block block);
	static const BlockData& getBlockData(size_t index);
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