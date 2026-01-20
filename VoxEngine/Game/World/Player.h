#pragma once
#include "Graphics/Camera.h"

#include "Game/DataPackManagment/DataTypes/BlockData.h"
#include "Game/Item.h"

#include "RaycastResult.h"
#include "Entity.h"

#include "Input/InputManager.h"

enum class GameMode : uint8_t
{
	Normal,
	Fly
};

constexpr int PLAYER_HOTBAR_SIZE = 9;
constexpr int PLAYER_INVENTORY_SIZE = 27;

class Player : public Entity
{
	Camera camera;

	Item hotbar[PLAYER_HOTBAR_SIZE];
	uint8_t hotbarSelectedItemIndex = 0;

	Item inventory[PLAYER_INVENTORY_SIZE];

	GameMode gameMode = GameMode::Normal;

	InputManager input;
public:
	RaycastResult raycastResult;

	Player(const glm::dvec3& position, float yaw, float pitch);

	void update(double deltaTime) override;
private:
	void getMovingValues(double& friction, double& maxSpeed, double& maxAcceleration) const;
public:
	void interpolateCameraTransform(float factor);

	void setPosition(const glm::vec3& position);
	void setYaw(float yaw);
	void setPitch(float pitch);
	void setYawPitch(float yaw, float pitch);
	void setTransform(const Transform& transform);

	void setGameMode(GameMode gameMode);

	void move(const glm::vec3& delta);
	void rotate(float deltaYaw, float deltaPitch);

	glm::dvec3 getPosition() const { return transform.position; };
	float getYaw() const { return transform.yaw; };
	float getPitch() const { return transform.pitch; };
	Transform getTransform() const { return transform; };
	Transform getPreviousTransform() const { return previousTransform; };
	Camera& getCamera() { return camera; };
	const Camera& getCamera() const { return camera; };
	InputManager& getInputManager() { return input; }

	const Item* getHotbar() const { return hotbar; }
	int getSelectedItemIndex() const { return hotbarSelectedItemIndex; }
	const Item& getSelectedItem() const { return hotbar[hotbarSelectedItemIndex]; };
};

