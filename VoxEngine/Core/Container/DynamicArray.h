#pragma once
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>

template<typename T>
class DynamicArray
{
	T* mData = nullptr;
	size_t mSize = 0;
	size_t mCapacity = 0;

	void destroy_elements() noexcept
	{
		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			if (!mData) return;

			for (size_t i = 0; i < mSize; i++)
			{
				destruct_at(mData + i);
			}
		}
	}

	template<typename... Args>
	inline void construct_at(T* pos, Args&&... args)
	{
		new (pos) T(std::forward<Args>(args)...);
	}

	inline void destruct_at(T* pos)
	{
		(*pos).~T();
	}

	void changeCapacity(size_t newCapacity)
	{
		if (newCapacity == 0)
		{
			destroy_elements();
			::operator delete(mData);
			mData = nullptr;
			mSize = 0;
			mCapacity = 0;
			return;
		}

		T* newData = static_cast<T*>(::operator new(newCapacity * sizeof(T)));

		if constexpr (std::is_trivially_move_constructible_v<T>)
		{
			std::memcpy(newData, mData, mSize * sizeof(T));
		}
		else
		{
			size_t i = 0;
			try
			{
				for (; i < mSize; i++)
				{
					construct_at(newData + i, std::move(mData[i]));
				}
			}
			catch (...)
			{
				for (size_t j = 0; j < i; j++)
				{
					destruct_at(newData + j);
				}
				::operator delete(newData);
				throw;
			}
		}

		destroy_elements();
		::operator delete(mData);
		mData = newData;
		mCapacity = newCapacity;
	}
public:
	using value_type = T;
	using size_type = size_t;
	using iterator = T*;
	using const_iterator = const T*;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	// Creation, destruction, copying, moving
	DynamicArray() noexcept : mData(nullptr), mSize(0), mCapacity(0) {}

	explicit DynamicArray(size_t size)
	{
		if (size == 0) return;

		mData = static_cast<T*>(::operator new(size * sizeof(T)));
		mSize = size;
		mCapacity = size;

		size_t i = 0;
		try
		{
			for (; i < size; i++)
			{
				construct_at(mData + i);
			}
		}
		catch (...)
		{
			for (size_t j = 0; j < i; j++)
			{
				destruct_at(mData + j);
			}
			::operator delete(mData);
			throw;
		}
	}

	explicit DynamicArray(size_t size, const T& value)
	{
		if (size == 0) return;

		mData = static_cast<T*>(::operator new(size * sizeof(T)));
		mSize = size;
		mCapacity = size;

		size_t i = 0;
		try
		{
			for (; i < size; i++)
			{
				construct_at(mData + i, value);
			}
		}
		catch (...)
		{
			for (size_t j = 0; j < i; j++)
			{
				destruct_at(mData + j);
			}
			::operator delete(mData);
			throw;
		}
	}

	explicit DynamicArray(const T* begin, const T* end)
	{
		if (begin == nullptr || end == nullptr)
		{
			throw std::invalid_argument("Null pointer provided to DynamicArray constructor");
		}
		else if (end <= begin)
		{
			throw std::invalid_argument("Invalid range: end must be greater than begin");
		}

		mSize = end - begin;

		if (mSize == 0) return;

		mCapacity = mSize;
		mData = static_cast<T*>(::operator new(mCapacity * sizeof(T)));

		if constexpr (std::is_trivially_copy_constructible_v<T>)
		{
			std::memcpy(mData, begin, mSize * sizeof(T));
		}
		else
		{
			size_t i = 0;
			try
			{
				for (; i < mSize; i++)
				{
					construct_at(mData + i, begin[i]);
				}
			}
			catch (...)
			{
				for (size_t j = 0; j < i; j++)
				{
					destruct_at(mData + j);
				}
				::operator delete(mData);
				throw;
			}
		}
	}

	DynamicArray(std::initializer_list<T> init)
	{
		mSize = init.size();

		if (mSize == 0) return;

		mCapacity = mSize;
		mData = static_cast<T*>(::operator new(mCapacity * sizeof(T)));

		if constexpr (std::is_trivially_copy_constructible_v<T>)
		{
			std::memcpy(mData, init.begin(), mSize * sizeof(T));
		}
		else
		{
			size_t i = 0;
			try
			{
				for (; i < mSize; i++)
				{
					construct_at(mData + i, init[i]);
				}
			}
			catch (...)
			{
				for (size_t j = 0; j < i; j++)
				{
					destruct_at(mData + j);
				}
				::operator delete(mData);
				throw;
			}
		}
	}

	template<typename InputIt>
	//requires (!std::is_same_v<std::decay_t<InputIt>, DynamicArray>)
	DynamicArray(InputIt first, InputIt last)
	{
		mSize = 0;
		for (InputIt it = first; it != last; ++it)
		{
			mSize++;
		}

		if (mSize == 0) return;

		mCapacity = mSize;
		mData = static_cast<T*>(::operator new(mCapacity * sizeof(T)));

		size_t i = 0;
		try
		{
			for (InputIt it = first; it != last; ++it)
			{
				construct_at(mData + i, *it);
				i++;
			}
		}
		catch (...)
		{
			for (size_t j = 0; j < i; j++)
			{
				destruct_at(mData + j);
			}
			::operator delete(mData);
			throw;
		}
	}

	~DynamicArray()
	{
		if (mData)
		{
			destroy_elements();
			::operator delete(mData);
		}
	}

	DynamicArray(const DynamicArray& other)
	{
		mSize = other.mSize;
		if (other.mCapacity > 0)
		{
			mCapacity = other.mCapacity;
			mData = static_cast<T*>(::operator new(mCapacity * sizeof(T)));
		}

		if (mSize == 0) return;

		if (std::is_trivially_copy_constructible_v<T>)
		{
			std::memcpy(mData, other.mData, mSize * sizeof(T));
		}
		else
		{
			size_t i = 0;
			try
			{
				for (; i < mSize; i++)
				{
					construct_at(mData + i, other.mData[i]);
				}
			}
			catch (...)
			{
				for (size_t j = 0; j < i; j++)
				{
					destruct_at(mData + j);
				}
				::operator delete(mData);
				throw;
			}
		}
	}

	DynamicArray& operator=(const DynamicArray& other)
	{
		if (this != &other)
		{
			DynamicArray temp(other);
			swap(temp);
		}
		return *this;
	}

	DynamicArray(DynamicArray&& other) noexcept :
		mData(other.mData),
		mSize(other.mSize),
		mCapacity(other.mCapacity)
	{
		other.mData = nullptr;
		other.mSize = 0;
		other.mCapacity = 0;
	}

	DynamicArray& operator=(DynamicArray&& other) noexcept
	{
		if (this != &other)
		{
			if (mData)
			{
				destroy_elements();
				::operator delete(mData);
			}

			mData = other.mData;
			mSize = other.mSize;
			mCapacity = other.mCapacity;

			other.mData = nullptr;
			other.mSize = 0;
			other.mCapacity = 0;
		}
		return *this;
	}

	// Swap

	void swap(DynamicArray& other) noexcept
	{
		std::swap(mData, other.mData);
		std::swap(mSize, other.mSize);
		std::swap(mCapacity, other.mCapacity);
	}

	// Element access

	[[nodiscard]] T& operator[](size_t index) noexcept { return mData[index]; }

	[[nodiscard]] const T& operator[](size_t index) const noexcept { return mData[index]; }

	[[nodiscard]] T& at(size_t index)
	{
		if (index >= mSize)
		{
			throw std::out_of_range("Index " + std::to_string(index) + " is out of range. DynamicArray size is " + std::to_string(mSize));
		}
		return mData[index];
	}

	[[nodiscard]] const T& at(size_t index) const
	{
		if (index >= mSize)
		{
			throw std::out_of_range("Index " + std::to_string(index) + " is out of range. DynamicArray size is " + std::to_string(mSize));
		}
		return mData[index];
	}

	[[nodiscard]] T& front()
	{
		if (mSize == 0)
		{
			throw std::out_of_range("front() called on empty DynamicArray");
		}
		return mData[0];
	}

	[[nodiscard]] const T& front() const
	{
		if (mSize == 0)
		{
			throw std::out_of_range("front() called on empty DynamicArray");
		}
		return mData[0];
	}

	[[nodiscard]] T& back()
	{
		if (mSize == 0)
		{
			throw std::out_of_range("back() called on empty DynamicArray");
		}
		return mData[mSize - 1];
	}

	[[nodiscard]] const T& back() const
	{
		if (mSize == 0)
		{
			throw std::out_of_range("back() called on empty DynamicArray");
		}
		return mData[mSize - 1];
	}

	// Modifiers

	void clear()
	{
		destroy_elements();
		mSize = 0;
	}

	void reserve(size_t newCapacity)
	{
		if (newCapacity > mCapacity)
		{
			changeCapacity(newCapacity);
		}
	}

	void resize(size_t newSize)
	{
		if (newSize > mSize)
		{
			if (newSize > mCapacity)
			{
				changeCapacity(newSize);
			}

			for (size_t i = mSize; i < newSize; i++)
			{
				construct_at(mData + i);
			}
		}
		else if (newSize < mSize)
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				for (size_t i = newSize; i < mSize; i++)
				{
					destruct_at(mData + i);
				}
			}
		}
		mSize = newSize;
	}

	void resize(size_t newSize, const T& value)
	{
		if (newSize > mSize)
		{
			if (newSize > mCapacity)
			{
				changeCapacity(newSize);
			}

			for (size_t i = mSize; i < newSize; i++)
			{
				construct_at(mData + i, value);
			}
		}
		else if (newSize < mSize)
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				for (size_t i = newSize; i < mSize; i++)
				{
					destruct_at(mData + i);
				}
			}
		}
		mSize = newSize;
	}

	void fill(const T& value)
	{
		for (size_t i = 0; i < mSize; i++)
		{
			mData[i] = value;
		}
	}

	template<typename... Args>
	T& emplace_back(Args&&... args)
	{
		if (mSize >= mCapacity)
		{
			size_t newCapacity = mCapacity * 2;
			if (newCapacity == 0) newCapacity = 1;
			changeCapacity(newCapacity);
		}

		construct_at(mData + mSize, std::forward<Args>(args)...);

		return mData[mSize++];
	}

	template<typename... Args>
	T& emplace(size_t index, Args&&... args)
	{
		if (index > mSize) throw std::out_of_range("DynamicArray::emplace: position out of range");

		if (mSize >= mCapacity)
		{
			size_t newCapacity = mCapacity * 2;
			if (newCapacity == 0) newCapacity = 1;
			changeCapacity(newCapacity);
		}

		if (std::is_trivially_move_constructible_v<T>)
		{
			std::memmove(mData + index + 1, mData + index, (mSize - index) * sizeof(T));
		}
		else
		{
			for (size_t i = mSize; i > index; --i)
			{
				construct_at(mData + i, std::move(mData[i - 1]));
				mData[i - 1].~T();
			}
		}

		construct_at(mData + index, std::forward<Args>(args)...);

		mSize++;
		return mData[index];
	}

	template<typename... Args>
	iterator emplace(const_iterator pos, Args&&... args)
	{
		const size_t index = pos - cbegin();
		emplace(index, std::forward<Args>(args)...);
		return begin() + index;
	}

	void push_back(const T& value)
	{
		emplace_back(value);
	}

	void push_back(T&& value)
	{
		emplace_back(std::move(value));
	}

	void insert(size_t index, const T& value)
	{
		emplace(index, value);
	}

	void insert(size_t index, T&& value)
	{
		emplace(index, std::move(value));
	}

	iterator insert(const_iterator pos, const T& value)
	{
		const size_t index = pos - cbegin();
		emplace(index, value);
		return begin() + index;
	}

	iterator insert(const_iterator pos, T&& value)
	{
		const size_t index = pos - cbegin();
		emplace(index, std::move(value));
		return begin() + index;
	}

	void erase(size_t index)
	{
		if (index >= mSize)
			throw std::out_of_range("DynamicArray::erase: position out of range");

		if constexpr (std::is_trivially_destructible_v<T>)
		{
			std::memmove(mData + index, mData + index + 1, (mSize - index - 1) * sizeof(T));
		}
		else
		{
			mData[index].~T();
			for (size_t i = index + 1; i < mSize; i++)
			{
				construct_at(mData + i - 1, std::move(mData[i]));
				mData[i].~T();
			}
		}
		mSize--;
	}

	void erase(size_t start, size_t end)
	{
		if (start > mSize || end > mSize || start > end)
			throw std::out_of_range("DynamicArray::erase: invalid range");

		size_t count = end - start;
		if (count == 0)
			return;

		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			for (size_t i = start; i < end; i++)
				mData[i].~T();
		}

		if constexpr (std::is_trivially_move_constructible_v<T>)
		{
			std::memmove(mData + start, mData + end, (mSize - end) * sizeof(T));
		}
		else
		{
			for (size_t i = end; i < mSize; i++)
			{
				construct_at(mData + i - count, std::move(mData[i]));
				mData[i].~T();
			}
		}
		mSize -= count;
	}

	iterator erase(const_iterator pos)
	{
		const size_t index = pos - cbegin();
		erase(index);
		return begin() + index;
	}

	iterator erase(const_iterator first, const_iterator last)
	{
		size_t start = first - cbegin();
		size_t end = last - cbegin();

		if (start > mSize || end > mSize || start > end)
			throw std::out_of_range("DynamicArray::erase: invalid range");

		size_t count = end - start;
		if (count == 0)
			return begin() + start;

		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			for (size_t i = start; i < end; ++i)
				mData[i].~T();
		}

		if constexpr (std::is_trivially_move_constructible_v<T>)
		{
			std::memmove(mData + start, mData + end, (mSize - end) * sizeof(T));
		}
		else
		{
			for (size_t i = end; i < mSize; i++)
			{
				construct_at(mData + i - count, std::move(mData[i]));
				mData[i].~T();
			}
		}
		mSize -= count;

		return begin() + start;
	}

	void pop_back()
	{
		if (mSize == 0)
		{
			throw std::out_of_range("DynamicArray::pop_back(): cannot pop from empty array");
		}

		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			mData[--mSize].~T();
		}
		else
		{
			mSize--;
		}
	}

	void shrink_to_fit()
	{
		if (mCapacity > mSize)
		{
			changeCapacity(mSize);
		}
	}

	// Iterator support

	[[nodiscard]] iterator begin() noexcept { return mData; }
	[[nodiscard]] iterator end() noexcept { return mData + mSize; }

	[[nodiscard]] const_iterator begin() const noexcept { return mData; }
	[[nodiscard]] const_iterator end() const noexcept { return mData + mSize; }
	[[nodiscard]] const_iterator cbegin() const noexcept { return mData; }
	[[nodiscard]] const_iterator cend() const noexcept { return mData + mSize; }

	[[nodiscard]] reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	[[nodiscard]] reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

	[[nodiscard]] const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	[[nodiscard]] const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
	[[nodiscard]] const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
	[[nodiscard]] const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

	// Getters

	[[nodiscard]] T* data() const noexcept { return mData; }
	[[nodiscard]] size_t size() const noexcept { return mSize; }
	[[nodiscard]] size_t capacity() const noexcept { return mCapacity; }
	[[nodiscard]] bool empty() const noexcept { return mSize == 0; }
};