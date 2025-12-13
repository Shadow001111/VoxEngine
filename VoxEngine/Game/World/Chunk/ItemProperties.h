#pragma once
#include <string>
#include <cstdint>

enum class ItemType : uint8_t
{
    BLOCK,      // Can be placed as a block
    TOOL,       // Used for interactions
    CONSUMABLE, // Can be consumed (food, potions)
    OTHER
};

struct ItemProperties
{
    std::string displayName;
    ItemType type = ItemType::OTHER;
    uint16_t maxStackSize = 64; // Default Minecraft-style stack size
    uint16_t durability = 0; // 0 = unbreakable

    // For tools
    ToolType toolType = ToolType::NONE;
    ToolMaterial toolMaterial = ToolMaterial::NONE;
    float miningSpeed = 1.0f;
    uint8_t miningLevel = 0;
    float attackDamage = 0.0f;

    // For consumables
    int16_t foodValue = 0; // Hunger points restored
    float saturation = 0.0f;

    // For blocks
    std::string placedBlockName; // Block to place when used

    // Visual properties
    std::string textureName;
    std::string modelName;

    ItemProperties() = default;
    ItemProperties(
        const std::string& displayName,
        ItemType type,
        uint16_t maxStackSize = 64
    );
};