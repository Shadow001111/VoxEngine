#pragma once
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>

// TODO: Add [[nodiscard]]

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
			for (size_t i = 0; i < mSize; i++)
			{
				mData[i].~T();
			}
		}
	}

	//template<typename... Args>
	//void construct_at(size_t pos, Args&&... args)
	//{
	//	new (&mData[pos]) T(std::forward<Args>(args)...);
	//}

	void changeCapacity(size_t newCapacity)
	{
		if (newCapacity == 0) newCapacity = 1;

		T* newData = static_cast<T*>(::operator new(newCapacity * sizeof(T)));
		size_t i = 0;

		try
		{
			for (; i < mSize; i++)
			{
				new (&newData[i]) T(std::move(mData[i]));
			}
		}
		catch (...)
		{
			for (size_t j = 0; j < i; j++)
			{
				newData[j].~T();
			}
			::operator delete(newData);
			throw;
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

	explicit DynamicArray() :
		mData(static_cast<T*>(::operator new(sizeof(T)))),
		mSize(0),
		mCapacity(1)
	{
	}

	explicit DynamicArray(size_t size) :
		mData(static_cast<T*>(::operator new(size * sizeof(T)))),
		mSize(size),
		mCapacity(size)
	{
		for (size_t i = 0; i < size; i++)
		{
			new (&mData[i]) T();
		}
	}

	explicit DynamicArray(size_t size, const T& value) :
		mData(static_cast<T*>(::operator new(size * sizeof(T)))),
		mSize(size),
		mCapacity(size)
	{
		for (size_t i = 0; i < size; i++)
		{
			new (&mData[i]) T(value);
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
		mCapacity = mSize;
		mData = static_cast<T*>(::operator new(mCapacity * sizeof(T)));

		size_t i = 0;
		try
		{
			for (; i < mSize; i++)
			{
				new (&mData[i]) T(begin[i]);
			}
		}
		catch (...)
		{
			for (size_t j = 0; j < i; j++)
			{
				mData[j].~T();
			}
			::operator delete(mData);
			throw;
		}
	}

	DynamicArray(std::initializer_list<T> init)
	{
		mCapacity = init.size() > 0 ? init.size() : 1;
		mData = static_cast<T*>(::operator new(mCapacity * sizeof(T)));

		size_t i = 0;
		try
		{
			for (const auto& value : init)
			{
				new (&mData[i]) T(value);
				i++;
			}
			mSize = i;
		}
		catch (...)
		{
			for (size_t j = 0; j < i; j++)
			{
				mData[j].~T();
			}
			::operator delete(mData);
			throw;
		}
	}

	template<typename InputIt>
	DynamicArray(InputIt first, InputIt last)
	{
		// First, determine the distance (works for all iterator categories)
		size_t count = 0;
		for (InputIt it = first; it != last; ++it)
		{
			count++;
		}

		mCapacity = count > 0 ? count : 1;
		mData = static_cast<T*>(::operator new(mCapacity * sizeof(T)));

		size_t i = 0;
		try
		{
			for (InputIt it = first; it != last; ++it)
			{
				new (&mData[i]) T(*it);
				i++;
			}
			mSize = i;
		}
		catch (...)
		{
			for (size_t j = 0; j < i; j++)
			{
				mData[j].~T();
			}
			::operator delete(mData);
			throw;
		}
	}

	~DynamicArray()
	{
		destroy_elements();
		::operator delete(mData);
	}

	DynamicArray(const DynamicArray& other)
	{
		mSize = other.mSize;
		mCapacity = other.mCapacity;
		mData = static_cast<T*>(::operator new(mCapacity * sizeof(T)));

		size_t i = 0;
		try
		{
			for (; i < mSize; ++i)
			{
				new (&mData[i]) T(other.mData[i]);
			}
		}
		catch (...)
		{
			for (size_t j = 0; j < i; ++j)
			{
				mData[j].~T();
			}
			::operator delete(mData);
			throw;
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
			destroy_elements();
			::operator delete(mData);

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

	T& operator[](size_t index) noexcept { return mData[index]; }

	const T& operator[](size_t index) const noexcept { return mData[index]; }

	T& at(size_t index)
	{
		if (index >= mSize)
		{
			throw std::out_of_range("Index " + std::to_string(index) + " is out of range. DynamicArray size is " + std::to_string(mSize));
		}
		return mData[index];
	}

	const T& at(size_t index) const
	{
		if (index >= mSize)
		{
			throw std::out_of_range("Index " + std::to_string(index) + " is out of range. DynamicArray size is " + std::to_string(mSize));
		}
		return mData[index];
	}

	T& front()
	{
		if (mSize == 0)
		{
			throw std::out_of_range("front() called on empty DynamicArray");
		}
		return mData[0];
	}

	const T& front() const
	{
		if (mSize == 0)
		{
			throw std::out_of_range("front() called on empty DynamicArray");
		}
		return mData[0];
	}

	T& back()
	{
		if (mSize == 0)
		{
			throw std::out_of_range("back() called on empty DynamicArray");
		}
		return mData[mSize - 1];
	}

	const T& back() const
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
				new (&mData[i]) T();
			}
		}
		else if (newSize < mSize)
		{
			for (size_t i = newSize; i < mSize; i++)
			{
				mData[i].~T();
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
				new (&mData[i]) T(value);
			}
		}
		else if (newSize < mSize)
		{
			for (size_t i = newSize; i < mSize; i++)
			{
				mData[i].~T();
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

	void push_back(const T& value)
	{
		emplace_back(value);
	}

	void push_back(T&& value)
	{
		emplace_back(std::move(value));
	}

	template<typename... Args>
	T& emplace_back(Args&&... args)
	{
		if (mSize >= mCapacity)
		{
			size_t newCapacity = mCapacity * 2;
			changeCapacity(newCapacity);
		}

		new (&mData[mSize]) T(std::forward<Args>(args)...);

		return mData[mSize++];
	}

	template<typename... Args>
	void insert_emplace(size_t index, Args&&... args)
	{
		if (index > mSize)
		{
			throw std::out_of_range("DynamicArray::insert(): position out of range");
		}

		if (mSize >= mCapacity)
		{
			size_t newCapacity = mCapacity * 2;
			changeCapacity(newCapacity);
		}

		// Move elements after insertion point one position to the right
		for (size_t i = mSize; i > index; --i)
		{
			new (&mData[i]) T(std::move(mData[i - 1]));
			mData[i - 1].~T();
		}

		// Construct new element at insertion point
		new (&mData[index]) T(std::forward<Args>(args)...);
		mSize++;
	}

	void insert(size_t index, const T& value)
	{
		insert_emplace(index, value);
	}

	void insert(size_t index, T&& value)
	{
		insert_emplace(index, std::move(value));
	}

	//void insert(size_t index, const T& value)
	//{
	//	if (index > mSize)
	//	{
	//		throw std::out_of_range("DynamicArray::insert(): position out of range");
	//	}
	//}

	void pop_back()
	{
		if (mSize == 0)
		{
			throw std::out_of_range("DynamicArray::pop_back(): cannot pop from empty array");
		}

		mData[--mSize].~T();
	}

	void shrink_to_fit()
	{
		if (mCapacity > mSize)
		{
			changeCapacity(mSize);
		}
	}

	// Iterator support

	iterator begin() noexcept { return mData; }
	iterator end() noexcept { return mData + mSize; }

	const_iterator begin() const noexcept { return mData; }
	const_iterator end() const noexcept { return mData + mSize; }
	const_iterator cbegin() const noexcept { return mData; }
	const_iterator cend() const noexcept { return mData + mSize; }

	reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

	const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
	const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
	const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

	// Getters

	T* data() const noexcept { return mData; }
	size_t size() const noexcept { return mSize; }
	size_t capacity() const noexcept { return mCapacity; }
	bool empty() const noexcept { return mSize == 0; }
};