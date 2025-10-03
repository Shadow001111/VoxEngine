#pragma once
class UpdateTimer
{
	float accumulatedTime = 0.0f;
	float updateInterval = 0.0f;
public:
	UpdateTimer(int updatesPerSecond);

	void addTime(float deltaTime);
	bool shouldUpdate();

	float getAccumulatedTime() const;
	float getAccumulatedTimeInPercent() const;
	float getUpdateInterval() const;
};

