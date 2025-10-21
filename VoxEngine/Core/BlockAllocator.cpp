#include "BlockAllocator.h"

#include <algorithm>
#include <iostream>

std::optional<size_t> BlockAllocator::findFreeBlock(size_t requestedSize) const
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

    // Check gaps between blocks
    for (size_t i = 0; i < blocks.size() - 1; i++)
    {
        const auto& block1 = blocks[i];
        size_t gapStart = block1.offset + block1.size;
        size_t gapEnd = blocks[i + 1].offset;
        size_t gapSize = gapEnd - gapStart;

        if (gapSize >= requestedSize)
        {
            return gapStart;
        }
    }

    // Check space at the end
    const auto& lastBlock = blocks.back();
    size_t lastEnd = lastBlock.offset + lastBlock.size;
    if (lastEnd + requestedSize <= capacity)
    {
        return lastEnd;
    }

    return std::nullopt;
}

BlockAllocator::BlockAllocator(size_t capacity) :
    capacity(capacity)
{
}

std::optional<BlockAllocator::Block> BlockAllocator::allocate(size_t size)
{
    if (size == 0)
    {
        return std::nullopt;
    }

    auto offset = findFreeBlock(size);
    if (!offset.has_value())
    {
        return std::nullopt; // Not enough space
    }

    Block block{ offset.value(), size, nextBlockID++ };

    // Insert block in sorted position by offset
    auto insertPos = std::lower_bound(blocks.begin(), blocks.end(), block,
        [](const Block& a, const Block& b) {
            return a.offset < b.offset;
        });
    blocks.insert(insertPos, block);

    return block;
}

bool BlockAllocator::free(size_t id)
{
    // TODO: Maybe use binary search?
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

bool BlockAllocator::setCapacity(size_t newCapacity)
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

void BlockAllocator::organizeAllocations()
{
    size_t lastEnd = 0;
    for (auto& block : blocks)
    {
        block.offset = lastEnd;
        lastEnd = block.offset + block.size;
    }
}

size_t BlockAllocator::getCapacity() const
{
    return capacity;
}

size_t BlockAllocator::getLastBlockEnd() const
{
    if (blocks.empty())
    {
        return 0;
    }
    const auto& lastBlock = blocks.back();
    return lastBlock.offset + lastBlock.size;
}

size_t BlockAllocator::getGapSizesSum() const
{
    size_t gaps = 0;
    size_t lastEnd = 0;
    for (const auto& block : blocks)
    {
        gaps += block.offset - lastEnd;
        lastEnd = block.offset + block.size;
    }
    return gaps;
}

const std::vector<BlockAllocator::Block>& BlockAllocator::getAllAllocations() const
{
    return blocks;
}
