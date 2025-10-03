#pragma once
class UpdateTimer
{
	float accumulatedTime = 0.0f;
	float updateInterval = 0.0f;
public:
	UpdateTimer(int updatesPerSecond);

	void addTime(float deltaTime);
	bool shouldUpdate();
	float getUpdateInterval() const;
};

