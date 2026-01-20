#include "InputManager.h"

void InputManager::onKey(int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        pressedKeys.insert(key);
    }
    else if (action == GLFW_RELEASE)
    {
        pressedKeys.erase(key);
    }
}

void InputManager::onMouseButton(int button, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        pressedMouseButtons.insert(button);
    }
    else if (action == GLFW_RELEASE)
    {
        pressedMouseButtons.erase(button);
    }
}

void InputManager::onMousePosition(double xpos, double ypos)
{
    mousePosition = glm::dvec2(xpos, ypos);

    if (firstMouse)
    {
        previousMousePosition = mousePosition;
        firstMouse = false;
    }

    mouseDelta += mousePosition - previousMousePosition;
    previousMousePosition = mousePosition;
}

void InputManager::onMouseScroll(double xoffset, double yoffset)
{
    scrollDelta += glm::dvec2(xoffset, yoffset);
}

void InputManager::processInput()
{
    // Process all keys that are in the keyStates map
    for (auto& [key, state] : keyStates)
    {
        // Store previous pressed state
        state.previousPressed = state.pressed;

        // Check if the key is currently pressed in the current frame
        bool currentlyPressed = pressedKeys.contains(key);

        // Update current pressed state
        state.pressed = currentlyPressed;

        // Determine justPressed: pressed now but wasn't pressed before
        state.justPressed = (state.pressed && !state.previousPressed);

        // Determine justReleased: not pressed now but was pressed before
        state.justReleased = (!state.pressed && state.previousPressed);
    }

    // Add new keys that were pressed but aren't in the keyStates map yet
    for (int key : pressedKeys)
    {
        if (!keyStates.contains(key))
        {
            KeyState newState;
            newState.pressed = 1;
            newState.justPressed = 1;  // First time we see this key, it's "just pressed"
            newState.justReleased = 0;
            newState.previousPressed = 0;
            keyStates[key] = newState;
        }
    }

    // Process all mouse buttons that are in the mouseButtonStates map
    for (auto& [button, state] : mouseButtonStates)
    {
        // Store previous pressed state
        state.previousPressed = state.pressed;

        // Check if the button is currently pressed in the current frame
        bool currentlyPressed = pressedMouseButtons.contains(button);

        // Update current pressed state
        state.pressed = currentlyPressed;

        // Determine justPressed: pressed now but wasn't pressed before
        state.justPressed = (state.pressed && !state.previousPressed);

        // Determine justReleased: not pressed now but was pressed before
        state.justReleased = (!state.pressed && state.previousPressed);
    }

    // Add new mouse buttons that were pressed but aren't in the mouseButtonStates map yet
    for (int button : pressedMouseButtons)
    {
        if (!mouseButtonStates.contains(button))
        {
            MouseButtonState newState;
            newState.pressed = 1;
            newState.justPressed = 1;  // First time we see this button, it's "just pressed"
            newState.justReleased = 0;
            newState.previousPressed = 0;
            mouseButtonStates[button] = newState;
        }
    }

    // Reset mouse delta for next frame
    returnMouseDelta = mouseDelta;
    mouseDelta = glm::dvec2(0.0);
}

bool InputManager::isKeyPressed(int key) const
{
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second.pressed;
}

bool InputManager::isKeyJustPressed(int key) const
{
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second.justPressed;
}

bool InputManager::isKeyJustReleased(int key) const
{
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second.justReleased;
}

bool InputManager::isMouseButtonPressed(int button) const
{
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() && it->second.pressed;
}

bool InputManager::isMouseButtonJustPressed(int button) const
{
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() && it->second.justPressed;
}

bool InputManager::isMouseButtonJustReleased(int button) const
{
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() && it->second.justReleased;
}