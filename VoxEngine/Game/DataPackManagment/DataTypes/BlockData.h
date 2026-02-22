#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "../ObjectTypes.h"

#include "Core/Container/DynamicArray.h"

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
	bool absorbsLight = false; // TODO: In chunk light propagation use this with culling to determine. Then it must to check current and next blocks' culling values.
	uint8_t lightEmission = 0;
	bool raycastable = true;

	bool hasFaces = false;
	bool faceCulling[6] = { false, false, false, false, false, false };

	// Visuals
	ModelId modelId = 0;
	DynamicArray<TextureSlot> textureSlots;

	// Sounds
	// TODO: Keep ids only
	std::vector<std::string> breakSounds;
	std::vector<std::string> placeSounds;
	std::vector<std::string> stepSounds;
};