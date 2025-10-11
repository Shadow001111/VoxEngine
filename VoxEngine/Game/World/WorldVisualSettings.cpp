#include "WorldVisualSettings.h"
#include <cmath>

float WorldVisualSettings::calculateFogDensity(float renderDistance_, float fogGradient_)
{
	return powf(-logf(1e-3f), 1.0f / fogGradient_) / renderDistance_;
}

float WorldVisualSettings::calculateFogGradient(float renderDistance_, float fogDensity_)
{
	return logf(-logf(1e-3f)) / logf(renderDistance_ * fogDensity_);
}