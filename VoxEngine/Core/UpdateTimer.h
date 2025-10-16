#pragma once
class UpdateTimer
{
	float accumulatedTime = 0.0f;
	float updateInterval = 0.0f;
public:
	UpdateTimer(float updatesPerSecond);

	void addTime(float deltaTime);
	void setUpdateToTrue();

	bool shouldUpdate();
	bool peek() const;

	float getAccumulatedTime() const;
	float getAccumulatedTimeInPercent() const;
	float getUpdateInterval() const;
};

