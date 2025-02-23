#pragma once

#include <type_traits>

#include "integral_types.h"

namespace PHX
{
	// Only valid for integral types and for vectors holding 2-4 elements
	template<typename T, unsigned N, class = typename std::enable_if<std::is_integral<T>::value && (N > 1 && N <= 4)>>
	class VecT
	{
	public:
		VecT()
		{
			std::memset(values, 0, N);
		}

		VecT(T v)
		{
			std::memset(values, static_cast<int>(v), N);
		}

		VecT(const std::initializer_list<T>& initList)
		{
			std::memset(values, 0, N);
			size_t copySize = initList.size() <= N ? initList.size() : N;
			std::copy_n(initList.begin(), copySize, values);
		}

		~VecT() = default;

		VecT(const VecT& other)
		{
			if (this != &other)
			{
				CopyValues(other);
			}
		}

		VecT(const VecT&& other) noexcept
		{
			CopyValues(other);
		}

		VecT& operator=(const VecT& other)
		{
			if (this == &other)
			{
				return *this;
			}

			CopyValues(other);
			return *this;
		}

		VecT& operator=(const std::initializer_list<T>& other)
		{
			if (other.size() == N)
			{
				std::copy_n(other.begin(), N, values);
			}

			return *this;
		}

		bool operator==(const VecT& other) const
		{
			return (std::memcmp(values, other.values, N * sizeof(T)) == 0);
		}

		template<class = typename std::enable_if<N >= 1>>
		T GetX() const { return values[0]; }

		template<class = typename std::enable_if<N >= 2>>
		T GetY() const { return values[1]; }

		template<class = typename std::enable_if<N >= 3>>
		T GetZ() const { return values[2]; }

		template<class = typename std::enable_if<N == 4>>
		T GetW() const { return values[3]; }

	private:

		T values[N];

		void CopyValues(const VecT& other)
		{
			std::memcpy(values, other.values, N * sizeof(T));
		}
	};

	// Template type aliases
	using Vec2f = VecT<float, 2>;
	using Vec2i = VecT<i32  , 2>;
	using Vec2u = VecT<u32  , 2>;

	using Vec3f = VecT<float, 3>;
	using Vec3i = VecT<i32  , 3>;
	using Vec3u = VecT<u32  , 3>;

	using Vec4f = VecT<float, 4>;
	using Vec4i = VecT<i32  , 4>;
	using Vec4u = VecT<u32  , 4>;
}