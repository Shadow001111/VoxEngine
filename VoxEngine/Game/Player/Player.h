#pragma once
#include "Graphics/Camera.h"

#include "Game/DataPackManagment/DataTypes/BlockData.h"
#include "Game/Item.h"
#include "Game/World/RaycastResult.h"
#include "Game/World/Entity.h"

#include "Input/InputManager.h"

#include "Game/Inventory/GUIInventory.h"

enum class GameMode : uint8_t
{
	Normal,
	Fly
};

// TODO: make private
struct InventoryDragState
{
	bool isDragging = false;
	Item draggedItem;
	size_t sourceSlot = 0;
	glm::vec2 dragStartPosition = glm::vec2(0.0f);
};

// TODO: Render dragged item
// TODO: Add dragging support for hotbar
class Player : public Entity
{
	static constexpr int PLAYER_HOTBAR_SIZE = 9;
	static constexpr int PLAYER_INVENTORY_SIZE = 27;

	Camera camera;

	GUIInventory hotbar;
	GUIInventory inventory;
	InventoryDragState dragState;
	uint8_t hotbarSelectedItemIndex = 0;
	bool inventoryOpened = false;

	GameMode gameMode = GameMode::Normal;

	InputManager input;
public:
	RaycastResult raycastResult;

	Player(const glm::dvec3& position, float yaw, float pitch);

	void update(double deltaTime) override;
private:
	void getMovingValues(double& friction, double& maxSpeed, double& maxAcceleration) const;

	void startDragging(size_t slot);
	void stopDragging(size_t slot);
	void processInventoryInput();
public:
	void interpolateCameraTransform(double factor);

	void setPosition(const glm::vec3& position);
	void setYaw(float yaw);
	void setPitch(float pitch);
	void setYawPitch(float yaw, float pitch);
	void setTransform(const Transform<double>& transform);

	void setGameMode(GameMode gameMode);

	void move(const glm::vec3& delta);
	void rotate(float deltaYaw, float deltaPitch);

	glm::dvec3 getPosition() const { return transform.position; };
	float getYaw() const { return transform.yaw; };
	float getPitch() const { return transform.pitch; };
	Transform<double> getTransform() const { return transform; };
	Transform<double> getPreviousTransform() const { return previousTransform; };
	Camera& getCamera() { return camera; };
	const Camera& getCamera() const { return camera; };
	InputManager& getInputManager() { return input; }

	const GUIInventory& getHotbar() const { return hotbar; }
	const GUIInventory& getInventory() const { return inventory; }

	auto getSelectedHotbarItemIndex() const { return hotbarSelectedItemIndex; }
	const Item& getSelectedItem() const { return *hotbar.getItemAt(hotbarSelectedItemIndex); };

	bool isInventoryOpened() const { return inventoryOpened; }

	const InventoryDragState& getDragState() const { return dragState; }
};
