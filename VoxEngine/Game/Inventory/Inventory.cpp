#include "Inventory.h"

void Inventory::configureStorage(size_t slotCount, size_t columnCount_)
{
	// Resize array
	items.resize(slotCount);

	// Set column count
	columnCount = columnCount_;

	// Calculate and set row count
	rowCount = ceilf((float)slotCount / (float)columnCount);
}

const Item* Inventory::getItemAt(size_t slotIndex) const
{
	if (slotIndex < items.size())
	{
		return &items[slotIndex];
	}
	return nullptr;
}

bool Inventory::pushItem(const Item& item)
{
	for (size_t i = 0; i < items.size(); i++)
	{
		Item& slotItem = items[i];

		if (slotItem.count == 0)
		{
			slotItem = item;
			return true;
		}
	}
	return false;
}

bool Inventory::putItem(const Item& item, size_t slotIndex, size_t swapIndex)
{
	if (slotIndex >= items.size())
	{
		return false;
	}

	Item& slotItem = items[slotIndex];
	if (slotItem.count > 0)
	{
		// Try to swap items
		if (swapIndex >= items.size())
		{
			return false;
		}

		Item& swapItem = items[swapIndex];
		if (swapItem.count > 0)
		{
			return pushItem(item);
		}

		swapItem = slotItem;
		slotItem = item;

		return true;
	}

	slotItem = item;
	return true;
}

std::optional<Item> Inventory::takeItem(size_t slotIndex)
{
	if (slotIndex >= items.size())
	{
		return std::nullopt;
	}

	auto item = items[slotIndex];
	if (item.count == 0)
	{
		return std::nullopt;
	}

	items[slotIndex].count = 0;
	return item;
}
