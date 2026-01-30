#include "GUIInventory.h"

void GUIInventory::configureVisualGrid(float leftBorder, float rightBorder, float borderY, bool gridGoesUp)
{
	visualGrid.leftBorder = leftBorder;
	visualGrid.rightBorder = rightBorder;
	visualGrid.borderY = borderY;
	visualGrid.gridGoesUp = gridGoesUp;
}

std::optional<size_t> GUIInventory::getSlotIndexAtPoint(const glm::vec2& point) const
{
	// Check if point is in bounds
	if (point.x < visualGrid.leftBorder || point.x > visualGrid.rightBorder)
	{
		return -1;
	}

	const float gridWidth = visualGrid.getWidth();
	const float slotSize = gridWidth / columnCount;

	float minY, maxY;
	if (visualGrid.gridGoesUp)
	{
		minY = visualGrid.borderY + slotSize;
		maxY = visualGrid.borderY + slotSize + rowCount * slotSize;
	}
	else
	{
		minY = visualGrid.borderY - rowCount * slotSize;
		maxY = visualGrid.borderY;
	}

	if (
		point.y < minY || point.y > maxY
		)
	{
		return -1;
	}

	// Calculate column index
	float nx = (point.x - visualGrid.leftBorder) / gridWidth;
	float ny = (point.y - minY) / (maxY - minY);

	int slotX = (int)floorf(nx * columnCount);
	slotX = std::min((int)columnCount - 1, slotX);

	int slotY = (int)floorf(ny * rowCount);
	if (visualGrid.gridGoesUp)
	{
		slotY = std::min((int)rowCount - 1, slotY);
	}
	else
	{
		slotY = std::max(0, (int)rowCount - 1 - slotY);
	}

	return slotX + slotY * columnCount;
}
