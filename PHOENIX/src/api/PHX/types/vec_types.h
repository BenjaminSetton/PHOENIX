#pragma once

#include <algorithm>
#include <initializer_list>
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
			std::memset(values, 0, GetSizeBytes());
		}

		VecT(T v)
		{
			std::memset(values, static_cast<int>(v), GetSizeBytes());
		}

		VecT(const std::initializer_list<T>& initList)
		{
			std::memset(values, 0, GetSizeBytes());
			size_t copySize = initList.size() <= N ? initList.size() : N;
			std::copy_n(initList.begin(), copySize, values);
		}

		template<class = typename std::enable_if<N >= 2>>
		VecT(T x, T y) { values[0] = x; values[1] = y; }

		template<class = typename std::enable_if<N >= 3>>
		VecT(T x, T y, T z) { values[0] = x; values[1] = y; values[2] = z; }

		template<class = typename std::enable_if<N >= 4>>
		VecT(T x, T y, T z, T w) { values[0] = x; values[1] = y; values[2] = z; values[3] = w; }

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
			return (std::memcmp(values, other.values, GetSizeBytes()) == 0);
		}

		template<class = typename std::enable_if<N >= 1>>
		T GetX() const { return values[0]; }

		template<class = typename std::enable_if<N >= 2>>
		T GetY() const { return values[1]; }

		template<class = typename std::enable_if<N >= 3>>
		T GetZ() const { return values[2]; }

		template<class = typename std::enable_if<N == 4>>
		T GetW() const { return values[3]; }

		template<class = typename std::enable_if<N >= 1>>
		void SetX(T val) { values[0] = val; }

		template<class = typename std::enable_if<N >= 2>>
		void SetY(T val) { values[1] = val; }

		template<class = typename std::enable_if<N >= 3>>
		void SetZ(T val) { values[2] = val; }

		template<class = typename std::enable_if<N == 4>>
		void SetW(T val) { values[3] = val; }

		constexpr u32 GetSize() const
		{
			return N;
		}

	private:

		T values[N];

		void CopyValues(const VecT& other)
		{
			std::memcpy(values, other.values, N * sizeof(T));
		}

		constexpr u32 GetSizeBytes()
		{
			return sizeof(T) * N;
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

#define DEFINE_HASH_2D_VEC(vecType, T)                        \
template<> struct hash<vecType> {                             \
	size_t operator()(const vecType& vec) const {             \
		size_t finalHash =  std::hash<T>()(vec.GetX());       \
		finalHash       ^= (std::hash<T>()(vec.GetY()) << 1); \
		return finalHash;                                     \
	}                                                         \
};

#define DEFINE_HASH_3D_VEC(vecType, T)                        \
template<> struct hash<vecType> {                             \
	size_t operator()(const vecType& vec) const {             \
		size_t finalHash =  std::hash<T>()(vec.GetX());       \
		finalHash       ^= (std::hash<T>()(vec.GetY()) << 1); \
		finalHash       ^= (std::hash<T>()(vec.GetZ()) << 2); \
		return finalHash;                                     \
	}                                                         \
};

#define DEFINE_HASH_4D_VEC(vecType, T)                        \
template<> struct hash<vecType> {                             \
	size_t operator()(const vecType& vec) const {             \
		size_t finalHash =  std::hash<T>()(vec.GetX());       \
		finalHash       ^= (std::hash<T>()(vec.GetY()) << 1); \
		finalHash       ^= (std::hash<T>()(vec.GetZ()) << 2); \
		finalHash       ^= (std::hash<T>()(vec.GetW()) << 3); \
		return finalHash;                                     \
	}                                                         \
};

// Template specialization to allow vector types to be used in std containers thru std::hash
namespace std
{
	DEFINE_HASH_2D_VEC(PHX::Vec2f, float);
	DEFINE_HASH_2D_VEC(PHX::Vec2i, PHX::i32);
	DEFINE_HASH_2D_VEC(PHX::Vec2u, PHX::u32);
	DEFINE_HASH_3D_VEC(PHX::Vec3f, float);
	DEFINE_HASH_3D_VEC(PHX::Vec3i, PHX::i32);
	DEFINE_HASH_3D_VEC(PHX::Vec3u, PHX::u32);
	DEFINE_HASH_4D_VEC(PHX::Vec4f, float);
	DEFINE_HASH_4D_VEC(PHX::Vec4i, PHX::i32);
	DEFINE_HASH_4D_VEC(PHX::Vec4u, PHX::u32);
}