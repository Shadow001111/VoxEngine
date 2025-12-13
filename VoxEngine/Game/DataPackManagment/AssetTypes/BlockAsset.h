#pragma once
#include <cstdint>
#include <string>
#include <vector>

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

	// {packName}:{blockName}
	std::string blockStringId;

	// Properties
	bool absorbsLight = false;
	uint8_t lightEmission = 0;
	bool raycastable = true;

	// Visuals
	std::string modelName;
	std::vector<TextureInfo> textureInfo;

	// Sounds
	std::vector<std::string> breakSounds;
	std::vector<std::string> placeSounds;
	std::vector<std::string> stepSounds;
};

