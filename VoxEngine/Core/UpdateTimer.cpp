#include "UpdateTimer.h"

UpdateTimer::UpdateTimer(int updatesPerSecond) :
	accumulatedTime(0.0f), updateInterval(updatesPerSecond > 0 ? 1.0f / (float)updatesPerSecond : 0.0f)
{
}

void UpdateTimer::addTime(float deltaTime)
{
	accumulatedTime += deltaTime;
}

bool UpdateTimer::shouldUpdate()
{
	if (accumulatedTime >= updateInterval)
	{
		accumulatedTime -= updateInterval;
		return true;
	}
	return false;
}

float UpdateTimer::getUpdateInterval() const
{
	return updateInterval;
}
