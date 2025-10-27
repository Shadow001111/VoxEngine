#pragma once
#include "Graphics/Camera.h"

#include "Chunk/Block.h"

#include "RaycastResult.h"
#include "Entity.h"

struct PlayerInput
{
	bool moveForward = false;
	bool moveBackward = false;
	bool moveLeft = false;
	bool moveRight = false;
	bool jump = false;
	bool crouch = false;
	bool sprint = false;

	bool numbers[10] = { false, false, false, false, false, false, false, false, false, false };

	bool leftMousePressed = false;
	bool rightMousePressed = false;

	glm::vec2 mouseDelta = glm::vec2(0.0f);
};

constexpr int PLAYER_HOTBAR_SIZE = 6;

class Player : public Entity
{
	Camera camera;

	Block hotbar[PLAYER_HOTBAR_SIZE] = {
		Block::GrassBlock,
		Block::Dirt,
		Block::Stone,
		Block::GlowStone,
		Block::Glass,
		Block::ColoredGlass
	};
	uint8_t selectedItemIndex = 0;
public:
	PlayerInput input;

	RaycastResult raycastResult;

	Player(const glm::dvec3& position, float yaw, float pitch);

	void update(double deltaTime) override;
private:
	void resetInput();
	void getMovingValues(double& friction, double& maxSpeed, double& maxAcceleration) const;
public:
	void interpolateCameraTransform(float factor);

	void setPosition(const glm::vec3& position);
	void setYaw(float yaw);
	void setPitch(float pitch);
	void setYawPitch(float yaw, float pitch);
	void setTransform(const Transform& transform);

	void move(const glm::vec3& delta);
	void rotate(float deltaYaw, float deltaPitch);

	glm::dvec3 getPosition() const;
	float getYaw() const;
	float getPitch() const;
	Transform getTransform() const;
	Transform getPreviousTransform() const;
	Camera& getCamera();

	Block getSelectedItem() const;
};

