#pragma once
#include <array>
#include <cstddef>
#include <type_traits>

template <size_t Bits, typename Word = unsigned int>
    requires (std::is_unsigned_v<Word>)
class Bitset
{
    static constexpr size_t BitsPerWord = sizeof(Word) * 8;
    static_assert((BitsPerWord& (BitsPerWord - 1)) == 0, "BitsPerWord must be power of two");

    static constexpr size_t WordShift = [] {
        size_t s = 0;
        size_t v = BitsPerWord;
        while (v > 1)
        {
            v >>= 1;
            ++s;
        }
        return s;
        }();

    static constexpr size_t WordMask = BitsPerWord - 1;

    static constexpr size_t WordCount =
        (Bits + BitsPerWord - 1) >> WordShift;

    static inline constexpr size_t wordIndex(size_t bit) noexcept
    {
        return bit >> WordShift;
    }

    static inline constexpr size_t bitOffset(size_t bit) noexcept
    {
        return bit & WordMask;
    }

    std::array<Word, WordCount> data{};
public:
    Bitset() noexcept
    {
        reset();
    }

    Bitset(const Bitset& other)
    {
        std::memcpy(data.data(), other.data(), WordCount * sizeof(Word));
    }

    Bitset& operator=(const Bitset& other)
    {
        if (this != &other)
        {
            for (size_t i = 0; i < WordCount; i++)
            {
                std::memcpy(data.data(), other.data(), WordCount * sizeof(Word));
            }
        }
        return *this;
    }

    void reset() noexcept
    {
        std::memset(data.data(), 0, WordCount * sizeof(Word));
    }

    void set(size_t index, bool value) noexcept
    {
        const size_t w = wordIndex(index);
        const Word mask = Word(1) << bitOffset(index);

        if (value)
        {
            data[w] |= mask;
        }
        else
        {
            data[w] &= ~mask;
        }
    }

    [[nodiscard]] bool read(size_t index) const noexcept
    {
        const size_t w = wordIndex(index);
        const Word mask = Word(1) << bitOffset(index);

        return (data[w] & mask) != 0;
    }

    [[nodiscard]] bool readAndSet(size_t index, bool value) noexcept
    {
        const size_t w = wordIndex(index);
        const Word mask = Word(1) << bitOffset(index);

        bool cond = (data[w] & mask) != 0;
        if (value)
        {
            data[w] |= mask;
        }
        else
        {
            data[w] &= ~mask;
        }
        return cond;
    }

    [[nodiscard]] bool any() const noexcept
    {
        for (const auto& w : data)
        {
            if (w != 0)
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool none() const noexcept
    {
        return !any();
    }

    [[nodiscard]] bool all() const noexcept
    {
        // Check all full words first
        for (size_t i = 0; i < Bits / BitsPerWord; i++)
            if (data[i] != ~Word(0)) return false;
 
        // Check remaining bits in the last partial word (if any)
        constexpr size_t remainder = Bits % BitsPerWord;
        if constexpr (remainder != 0)
        {
            constexpr Word mask = (Word(1) << remainder) - 1;
            if ((data[WordCount - 1] & mask) != mask) return false;
        }
 
        return true;
    }
};