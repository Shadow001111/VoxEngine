#include "Player.h"

#include "Game/World.h"
#include "Game/DataPackManagment/AssetRegistry.h"

#include "Core/Random.h"

glm::dvec3 makeVectorFlatNormalized(const glm::dvec3& vec)
{
	glm::dvec3 flat = vec;
	flat.y = 0.0;

	double squaredLength = glm::dot(flat, flat);
	if (squaredLength == 0.0)
	{
		return flat;
	}

	return flat / sqrt(squaredLength);
}

Player::Player(const glm::dvec3& position, float yaw, float pitch) :
	Entity(position, yaw, pitch, glm::dvec3(0.0), glm::dvec3(0.6, 1.7, 0.6), true),
	camera(position, yaw, pitch, glm::radians(90.0f), 1.0f, 0.01f, 1.0f)
{
	// Set game mode
	setGameMode(GameMode::Fly);

	// Configure hotbar
	hotbar.configureStorage(PLAYER_HOTBAR_SIZE, PLAYER_HOTBAR_SIZE);
	hotbar.configureVisualGrid(-0.9f, 0.9f, -1.0f, true);

	// Configure inventory
	inventory.configureStorage(PLAYER_INVENTORY_SIZE, 9);
	inventory.configureVisualGrid(-0.9f, 0.9f, 1.0f, false);

	// Fill hotbar
	for (size_t i = 0; i < hotbar.getSlotCount(); i++)
	{
		const auto* itemData = AssetRegistry::getItemData(i);
		if (itemData == nullptr)
		{
			break;
		}

		Item item;
		item.id = i;
		item.count = Random::integer<ItemId>(1, itemData->stackSize);
		hotbar.pushItem(item);
	}

	// Fill inventory
	for (size_t i = 0; i < inventory.getSlotCount(); i++)
	{
		const auto* itemData = AssetRegistry::getItemData(i);
		if (itemData == nullptr)
		{
			break;
		}

		Item item;
		item.id = i;
		item.count = Random::integer<ItemId>(1, itemData->stackSize);
		inventory.pushItem(item);
	}
}

void Player::update(double deltaTime)
{
	// Update base physics
	Entity::update(deltaTime);

	// Process input
	input.processInput();

	// Get input
	const bool moveRight = input.isKeyPressed(GLFW_KEY_D);
	const bool moveLeft = input.isKeyPressed(GLFW_KEY_A);
	const bool moveForward = input.isKeyPressed(GLFW_KEY_W);
	const bool moveBackward = input.isKeyPressed(GLFW_KEY_S);
	const bool jump = input.isKeyPressed(GLFW_KEY_SPACE);

	// Move
	if (!inventoryOpened)
	{
		if (gameMode == GameMode::Normal)
		{
			// Jump
			if (onGround && jump)
			{
				velocity.y = 10.0;
				onGround = false;
			}

			// Position and velocity
			{
				double friction, maxSpeed, maxAcceleration;
				getMovingValues(friction, maxSpeed, maxAcceleration);

				// Apply friction
				{
					double frictionForce = friction * deltaTime;
					glm::dvec3 flatVelocity = glm::dvec3(velocity.x, 0.0, velocity.z);
					if (frictionForce > glm::length(flatVelocity))
					{
						velocity.x = 0.0;
						velocity.z = 0.0;
					}
					else
					{
						velocity -= glm::normalize(flatVelocity) * frictionForce;
					}
				}

				// Get flat vectors
				glm::dvec3 right = makeVectorFlatNormalized(camera.getRight());
				glm::dvec3 forward = makeVectorFlatNormalized(camera.getForward());

				// Get wishDir
				double leftRight = moveRight - moveLeft;
				double forwardBackward = moveForward - moveBackward;
				glm::dvec3 wishDir = right * leftRight + forward * forwardBackward;

				if (glm::dot(wishDir, wishDir) > 0.0)
				{
					wishDir = glm::normalize(wishDir);

					double currentSpeed = glm::dot(velocity, wishDir);

					double acceleration = fmax(0.0, fmin(maxAcceleration * deltaTime, maxSpeed - currentSpeed));

					// Apply acceleration
					velocity += wishDir * acceleration;
				}
			}
		}
		else if (gameMode == GameMode::Fly)
		{
			double friction = 50.0, maxSpeed = 100.0, maxAcceleration = 100.0;

			// Apply friction
			{
				double frictionForce = friction * deltaTime;
				if (frictionForce > glm::length(velocity))
				{
					velocity = { 0.0, 0.0, 0.0 };
				}
				else
				{
					velocity -= glm::normalize(velocity) * frictionForce;
				}
			}

			// Get flat vectors
			glm::dvec3 right = camera.getRight();
			glm::dvec3 forward = camera.getForward();

			// Get wishDir
			double leftRight = moveRight - moveLeft;
			double forwardBackward = moveForward - moveBackward;

			glm::dvec3 wishDir = right * leftRight + forward * forwardBackward;
			if (jump)
			{
				wishDir += glm::dvec3(0.0f, 1.0f, 0.0f);
			}

			if (glm::dot(wishDir, wishDir) > 0.0)
			{
				wishDir = glm::normalize(wishDir);

				double currentSpeed = glm::dot(velocity, wishDir);

				double acceleration = fmax(0.0, fmin(maxAcceleration * deltaTime, maxSpeed - currentSpeed));

				// Apply acceleration
				velocity += wishDir * acceleration;
			}
		}
	}

	// Rotation
	if (!inventoryOpened)
	{
		const float mouseSensitivity = 0.002f;

		glm::dvec2 mouseDelta = input.getMouseDelta();

		rotate(-mouseDelta.x * mouseSensitivity, -mouseDelta.y * mouseSensitivity);
	}

	// Selecting hotbar item
	for (int i = 0; i < PLAYER_HOTBAR_SIZE; i++)
	{
		if (input.isKeyJustPressed(GLFW_KEY_1 + i))
		{
			hotbarSelectedItemIndex = i;
			break;
		}
	}

	{
		double scroll = input.getScrollDelta().y;

		if (scroll < 0.0)
		{
			hotbarSelectedItemIndex++;
			if (hotbarSelectedItemIndex >= PLAYER_HOTBAR_SIZE)
			{
				hotbarSelectedItemIndex = 0;
			}
		}
		else if (scroll > 0.0)
		{
			hotbarSelectedItemIndex--;
			if (hotbarSelectedItemIndex >= PLAYER_HOTBAR_SIZE)
			{
				hotbarSelectedItemIndex = PLAYER_HOTBAR_SIZE - 1;
			}
		}
	}

	// Process inventory input if inventory is opened
	if (inventoryOpened)
	{
		processInventoryInput();
	}

	// Raycast
	raycastResult = world->raycast(camera.getPosition(), camera.getForward(), 16.0f);
	if (raycastResult.hit && !inventoryOpened)
	{
		if (input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT))
		{
			const auto* item = hotbar.getItemAt(hotbarSelectedItemIndex);
			if (item)
			{
				const auto* itemData = AssetRegistry::getItemData(item->id);
				if (itemData && itemData->hasBlockPlaceable)
				{
					world->placeBlock(raycastResult, itemData->blockPlaceableId);
				}
			}
		}
		if (input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_RIGHT))
		{
			world->breakBlock(raycastResult);
		}
	}

	// Open/close inventory
	if (input.isKeyJustPressed(GLFW_KEY_I))
	{
		inventoryOpened = !inventoryOpened;
		// Clear drag state when closing inventory
		if (!inventoryOpened)
		{
			stopDragging(-1);
		}
	}
}

void Player::getMovingValues(double& friction, double& maxSpeed, double& maxAcceleration) const
{
	if (onGround)
	{
		const bool moveRight = input.isKeyPressed(GLFW_KEY_D);
		const bool moveLeft = input.isKeyPressed(GLFW_KEY_A);
		const bool moveForward = input.isKeyPressed(GLFW_KEY_W);
		const bool moveBackward = input.isKeyPressed(GLFW_KEY_S);
		const bool sprint = input.isKeyPressed(GLFW_KEY_LEFT_SHIFT);

		bool moveAny = moveForward || moveBackward || moveLeft || moveRight;
		if (sprint && moveAny)
		{
			friction = 40.0;
			maxSpeed = 10.0;
			maxAcceleration = 1000.0;
		}
		else
		{
			friction = 40.0;
			maxSpeed = 5.0;
			maxAcceleration = 500.0;
		}
	}
	else
	{
		friction = 1.0;
		maxSpeed = 3.0;
		maxAcceleration = 25.0;
	}
}

void Player::startDragging(size_t slot)
{
	auto sourceOpt = inventory.takeItem(slot);
	if (!sourceOpt.has_value())
	{
		return;
	}

	dragState.isDragging = true;
	dragState.draggedItem = sourceOpt.value();
	dragState.sourceSlot = slot;
}

void Player::stopDragging(size_t slot)
{
	if (!dragState.isDragging)
	{
		dragState = InventoryDragState();
		return;
	}

	bool success = inventory.putItem(dragState.draggedItem, slot, dragState.sourceSlot);
	if (!success)
	{
		inventory.pushItem(dragState.draggedItem);
	}

	dragState = InventoryDragState();
}

void Player::processInventoryInput()
{
	glm::dvec2 normalizedMousePos = input.getNormalizedMousePosition();

	// Start dragging on left click
	if (input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT))
	{
		auto slotOpt = inventory.getSlotIndexAtPoint(normalizedMousePos);

		if (slotOpt.has_value())
		{
			startDragging(slotOpt.value());
			dragState.dragStartPosition = glm::vec2(normalizedMousePos.x, normalizedMousePos.y);
		}
	}
	// Stop dragging on release
	else if (input.isMouseButtonJustReleased(GLFW_MOUSE_BUTTON_LEFT))
	{
		auto slotOpt = inventory.getSlotIndexAtPoint(normalizedMousePos);

		if (dragState.isDragging && slotOpt.has_value())
		{
			stopDragging(slotOpt.value());
		}
	}
}

void Player::interpolateCameraTransform(double factor)
{
	Transform interpolatedTransform = previousTransform.interpolate(transform, factor);
	interpolatedTransform.position.y += 1.7 * 0.5 - 0.2;
	camera.setTransform(interpolatedTransform);
}

void Player::setPosition(const glm::vec3& position)
{
	transform.position = position;
}

void Player::setYaw(float yaw)
{
	transform.yaw = yaw;
}

void Player::setPitch(float pitch)
{
	transform.pitch = glm::clamp(pitch, -1.5707f, 1.5707f);
}

void Player::setYawPitch(float yaw, float pitch)
{
	transform.yaw = yaw;
	transform.pitch = glm::clamp(pitch, -1.5707f, 1.5707f);
}

void Player::setTransform(const Transform<double>& transform)
{
	this->transform = transform;
	this->transform.pitch = glm::clamp(this->transform.pitch, -1.5707, 1.5707);
}

void Player::setGameMode(GameMode gameMode)
{
	this->gameMode = gameMode;
	if (gameMode == GameMode::Normal)
	{
		Entity::hasGravity = true;
	}
	else if (gameMode == GameMode::Fly)
	{
		Entity::hasGravity = false;
	}
}

void Player::move(const glm::vec3& delta)
{
	transform.position += delta;
}

void Player::rotate(float deltaYaw, float deltaPitch)
{
	transform.yaw += deltaYaw;
	transform.pitch += deltaPitch;
	transform.pitch = glm::clamp(transform.pitch, -1.5707, 1.5707);
}
