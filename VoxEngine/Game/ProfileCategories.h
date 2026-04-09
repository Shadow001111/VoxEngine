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
	ChunkColumnData,
	TerrainGeneration,
	__COUNT__
};