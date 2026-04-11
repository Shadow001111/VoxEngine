#pragma once
#include <cstdint>
#include <string>
#include "../ObjectTypes.h"

#include "Core/Container/DynamicArray.h"
#include "Core/Bitset.h"

// TODO: Reduce size
struct BlockData
{
	struct TextureSlot
	{
		enum class TextureTransformation : uint8_t
		{
			None = 0,
			Flip = 1,
			RotateAndFlip = 2
		};

		TextureId textureId = 0;
		TextureTransformation transformation = TextureTransformation::None;
		bool isTranslucent = false;
	};

	//
	std::string stringId;

	// Properties
	bool absorbsLight = false;
	uint8_t lightEmission = 0;
	bool raycastable = true;

	bool hasFaces = false;
	Bitset<6, uint8_t> faceCulling{};

	// Visuals
	ModelId modelId = 0;
	DynamicArray<TextureSlot> textureSlots;

	// Sounds
	// TODO: Keep ids only
	DynamicArray<std::string> breakSounds;
	DynamicArray<std::string> placeSounds;
	DynamicArray<std::string> stepSounds;
};