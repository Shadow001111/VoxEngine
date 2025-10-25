#pragma once
#include "Graphics/Camera.h"
#include "World/Chunk/Block.h"

struct PlayerInput
{
	bool moveForward = false;
	bool moveBackward = false;
	bool moveLeft = false;
	bool moveRight = false;
	bool moveUp = false;
	bool moveDown = false;
	bool sprint = false;

	bool numbers[10] = { false, false, false, false, false, false, false, false, false, false };

	bool leftMousePressed = false;
	bool rightMousePressed = false;

	glm::vec2 mouseDelta = glm::vec2(0.0f);
};

constexpr int PLAYER_HOTBAR_SIZE = 6;

class Player
{
	Transform transform;
	Transform previousTransform;

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
	Player(const glm::vec3& position, float yaw, float pitch);

	void updatePreviousTransform();
	void update(const PlayerInput& input, float deltaTime);
	void interpolateCameraTransform(float factor);

	void setPosition(const glm::vec3& position);
	void setYaw(float yaw);
	void setPitch(float pitch);
	void setYawPitch(float yaw, float pitch);
	void setTransform(const Transform& transform);

	void move(const glm::vec3& delta);
	void rotate(float deltaYaw, float deltaPitch);

	glm::vec3 getPosition() const;
	float getYaw() const;
	float getPitch() const;
	Transform getTransform() const;
	Transform getPreviousTransform() const;
	Camera& getCamera();

	Block getSelectedItem() const;
};

