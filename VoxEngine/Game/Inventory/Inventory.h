#pragma once
#include "Game/Item.h"
#include <vector>
#include <optional>

class Inventory
{
protected:
	std::vector<Item> items;
	uint32_t columnCount = 0, rowCount = 0;
public:
	void configureStorage(size_t slotCount, size_t columnCount);

	auto getColumnCount() const { return columnCount; }
	auto getRowCount() const { return rowCount; }
	auto getSlotCount() const { return items.size(); }

	const Item* getItemAt(size_t slotIndex) const; // Result is nullptr if slotIndex is out of range
	bool pushItem(const Item& item);
	bool putItem(const Item& item, size_t slotIndex, size_t swapIndex = 0);
	std::optional<Item> takeItem(size_t slotIndex); // Result has no value if there is no item at slotIndex or slotIndex is out of range

	// TODO: Implement inventory methods
	//void sortItems();
	//void defragment(); // Removes gaps in inventory by moving items
	//void defragmentAndGroup(); // Removes gaps in inventory by moving items; Items of same ID are grouped together
};

