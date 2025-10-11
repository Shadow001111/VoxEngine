#pragma once
#include "Core/RGB.h"

struct WorldVisualSettings
{
	RGB backgroundColor = {}; // Also fog color
	float fogMaxDistance = 0.0f; // Should be set as render distance
	float fogDensity = 0.0f;
	float fogGradient = 0.0f;

	static float calculateFogDensity(float renderDistance_, float fogGradient_);
	static float calculateFogGradient(float renderDistance_, float fogDensity_);
};
