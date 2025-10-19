#pragma once
#include <vector>
#include <optional>

class BlockAllocator
{
public:
	struct Block
	{
		size_t offset;
		size_t size;
		size_t id;

		bool operator==(const Block& other) const { return id == other.id && offset == other.offset && size == other.size; }
	};
private:
	size_t capacity;
	size_t nextBlockID = 0;
	std::vector<Block> blocks; // Always sorted

	std::optional<size_t> findFreeBlock(size_t requestedSize) const;
public:
	explicit BlockAllocator(size_t capacity);
	~BlockAllocator() = default;

	BlockAllocator(const BlockAllocator&) = delete;
	BlockAllocator& operator=(const BlockAllocator&) = delete;
	BlockAllocator(BlockAllocator&&) = delete;
	BlockAllocator& operator=(BlockAllocator&&) = delete;

	std::optional<Block> allocate(size_t size);
	bool free(size_t id);

	bool setCapacity(size_t newCapacity);

	void organizeAllocations();

	// Debug
	size_t getCapacity() const;
	size_t getLastBlockEnd() const;
	size_t getGapSizesSum() const;
	const std::vector<Block>& getAllAllocations() const;
};

