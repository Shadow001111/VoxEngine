#pragma once
#include <cstdint>
#include <string>
#include <array>
#include "Core/Container/DynamicArray.h"
#include "Core/Bitset.h"

struct BlockAsset
{
	struct TextureInfo
	{
		enum class TextureTransformation : uint8_t
		{
			None = 0,
			Flip = 1,
			RotateAndFlip = 2
		};

		std::string textureName;
		TextureTransformation transformation = TextureTransformation::None;
		bool isTranslucent = false;
	};

	//
	std::string stringId;

	// Properties
	Bitset<6, uint8_t> lightAbsorbing{};
	uint8_t lightEmission = 0;
	bool raycastable = true;

	// Visuals
	std::string modelName;
	DynamicArray<TextureInfo> textureInfo;

	// Sounds
	DynamicArray<std::string> breakSounds;
	DynamicArray<std::string> placeSounds;
	DynamicArray<std::string> stepSounds;
};

