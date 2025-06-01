#pragma once

namespace PHX
{
	template <typename T>
	inline void HashCombine(size_t& seed, const T& value)
	{
		seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	// TODO - untested
	//template<typename T>
	//struct ForwardIterator
	//{
	//	// Define the tags for STL container performance
	//	using iterator_category = std::forward_iterator_tag;
	//	using difference_type = std::ptrdiff_t;
	//	using value_type = T;
	//	using pointer = T*;
	//	using reference = T&;

	//	T& operator*() const { return *m_ptr; }
	//	T* operator->() { return m_ptr; }

	//	// Prefix increment
	//	ForwardIterator& operator++() { m_ptr++; return *this; }

	//	// Postfix increment
	//	ForwardIterator operator++(int) { ForwardIterator tmp = *this; ++(*this); return tmp; }

	//	friend bool operator== (const ForwardIterator& a, const ForwardIterator& b) { return a.m_ptr == b.m_ptr; };
	//	friend bool operator!= (const ForwardIterator& a, const ForwardIterator& b) { return a.m_ptr != b.m_ptr; };

	//private:

	//	T* m_ptr;
	//};
}