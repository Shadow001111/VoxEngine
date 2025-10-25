#include "UpdateTimer.h"

UpdateTimer::UpdateTimer(float updatesPerSecond) :
	accumulatedTime(0.0f), updateInterval(updatesPerSecond > 0.0f ? 1.0f / updatesPerSecond : 0.0f)
{
}

void UpdateTimer::addTime(float deltaTime)
{
	accumulatedTime += deltaTime;
}

void UpdateTimer::setUpdateToTrue()
{
	accumulatedTime = updateInterval;
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

int UpdateTimer::howManyTimesShouldUpdate()
{
	int count = 0;
	if (accumulatedTime >= updateInterval)
	{
		accumulatedTime -= updateInterval;
		count++;
	}
	return count;
}

bool UpdateTimer::peek() const
{
	return accumulatedTime >= updateInterval;
}

float UpdateTimer::getAccumulatedTime() const
{
	return accumulatedTime;
}

float UpdateTimer::getAccumulatedTimeInPercent() const
{
	return updateInterval > 0.0f ? (accumulatedTime / updateInterval) : 0.0f;
}

float UpdateTimer::getUpdateInterval() const
{
	return updateInterval;
}
