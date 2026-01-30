#pragma once
#include "Inventory.h"
#include <glm/glm.hpp>
#include <optional>

class GUIInventory : public Inventory
{
	struct VisualGrid
	{
		float leftBorder = 0.0f, rightBorder = 0.0f;
		float borderY = 0.0f;
		bool gridGoesUp = false;

		float getWidth() const { return rightBorder - leftBorder; }
	};

	VisualGrid visualGrid;
public:
	void configureVisualGrid(
		float leftBorder, float rightBorder, float borderY,
		bool gridGoesUp = false
	);

	std::optional<size_t> getSlotIndexAtPoint(const glm::vec2& point) const;

	const VisualGrid& getVisualGrid() const { return visualGrid; }
};

