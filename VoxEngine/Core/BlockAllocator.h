#pragma once
#include "Core/Container/DynamicArray.h"

#include <optional>
#include <concepts>

template<std::integral TIndex>
class BlockAllocator
{
public:
	struct Block
	{
		TIndex offset;
		TIndex size;
		TIndex id;

		bool operator==(const Block& other) const { return id == other.id && offset == other.offset && size == other.size; }
	};
private:
	TIndex capacity;
	TIndex nextBlockId = 0;
	DynamicArray<Block> blocks; // Always sorted by offset

	std::optional<TIndex> findFreeBlock(TIndex requestedSize) const
	{
		if (requestedSize > capacity)
		{
			return std::nullopt;
		}

		// Check if there's space at the beginning
		if (blocks.empty())
		{
			return 0;
		}
		if (blocks.front().offset >= requestedSize)
		{
			return 0;
		}

		// Check gaps between blocks
		for (size_t i = 0; i < blocks.size() - 1; i++)
		{
			const auto& block1 = blocks[i];
			const auto& block2 = blocks[i + 1];

			TIndex gapStart = block1.offset + block1.size;
			TIndex gapSize = block2.offset - gapStart;

			if (gapSize >= requestedSize)
			{
				return gapStart;
			}
		}

		// Check space at the end
		const auto& lastBlock = blocks.back();
		TIndex lastEnd = lastBlock.offset + lastBlock.size;
		if (lastEnd + requestedSize <= capacity)
		{
			return lastEnd;
		}

		return std::nullopt;
	}
public:
	explicit BlockAllocator(TIndex capacity) :
		capacity(capacity)
	{
	}

	~BlockAllocator() = default;

	BlockAllocator(const BlockAllocator&) = delete;
	BlockAllocator& operator=(const BlockAllocator&) = delete;
	BlockAllocator(BlockAllocator&&) = delete;
	BlockAllocator& operator=(BlockAllocator&&) = delete;

	std::optional<Block> allocate(TIndex size)
	{
		if (size == 0)
		{
			return std::nullopt;
		}

		auto offset = findFreeBlock(size);
		if (!offset.has_value())
		{
			return std::nullopt;
		}

		Block block{ offset.value(), size, nextBlockId++ };

		// Insert block in sorted position by offset
		auto insertPos = std::lower_bound(
			blocks.begin(), blocks.end(),
			block,
			[](const Block& a, const Block& b) {
				return a.offset < b.offset;
			});
		blocks.insert(insertPos, block);

		return block;
	}

	bool free(TIndex id)
	{
		auto it = std::find_if(blocks.begin(), blocks.end(),
			[id](const Block& block) {
				return block.id == id;
			});

		if (it != blocks.end())
		{
			blocks.erase(it);
			return true;
		}

		return false;
	}

	bool setCapacity(TIndex newCapacity)
	{
		if (newCapacity >= capacity)
		{
			capacity = newCapacity;
			return true;
		}

		// Check if current allocations fit in new capacity
		for (const auto& block : blocks)
		{
			if (block.offset + block.size > newCapacity)
			{
				return false; // Cannot shrink below current allocations
			}
		}

		capacity = newCapacity;
		return true;
	}

	void organizeAllocations()
	{
		TIndex lastEnd = 0;
		for (auto& block : blocks)
		{
			block.offset = lastEnd;
			lastEnd = block.offset + block.size;
		}
	}

	// Debug
	TIndex getCapacity() const noexcept { return capacity; }

	TIndex getLastBlockEnd() const noexcept
	{
		if (blocks.empty())
		{
			return 0;
		}
		const auto& lastBlock = blocks.back();
		return lastBlock.offset + lastBlock.size;
	}

	TIndex getGapSizesSum() const noexcept
	{
		TIndex gaps = 0;
		TIndex lastEnd = 0;
		for (const auto& block : blocks)
		{
			gaps += block.offset - lastEnd;
			lastEnd = block.offset + block.size;
		}
		return gaps;
	}

	const auto& getAllAllocations() const noexcept { return blocks; }
};