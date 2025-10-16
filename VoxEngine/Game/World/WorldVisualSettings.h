#pragma once
#include <glm/vec3.hpp>

struct WorldVisualSettings
{
	glm::vec3 backgroundColor = {}; // Also fog color
	float fogMaxDistance = 0.0f; // Should be set as render distance
	float fogDensity = 0.0f;
	float fogGradient = 0.0f;

	static float calculateFogDensity(float renderDistance_, float fogGradient_);
	static float calculateFogGradient(float renderDistance_, float fogDensity_);
};
