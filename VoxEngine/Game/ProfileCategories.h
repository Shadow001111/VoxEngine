#pragma once
#include <cstdint>

enum class ProfileCategory : uint64_t
{
	General,
	Render,
	ChunkLoadUnload,
	ChunkBlocks,
	ChunkLight,
	ChunkMesh,
	ChunkIO,
	ChunkColumnData,
	TerrainGeneration,
	Sound,
	__COUNT__
};