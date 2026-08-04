#pragma once

#include <type_traits> // std::is_integral_v

#include "PHX/types/integral_types.h"

namespace PHX
{
#define ENABLE_IF_INTEGRAL_TYPE(T) class = typename std::enable_if<std::is_integral_v<T>>::type

	template<typename T, ENABLE_IF_INTEGRAL_TYPE(T)>
	static T Min(const T left, const T right) noexcept { return (left < right ? left : right); }

	template<typename T, ENABLE_IF_INTEGRAL_TYPE(T)>
	static T Max(const T left, const T right) noexcept { return (left > right ? left : right); }

	template<typename T, ENABLE_IF_INTEGRAL_TYPE(T)>
	static T Clamp(T val, T min, T max) noexcept { return Min(Max(val, min), max); }

	// Aligns a value up to the next multiple of alignment. Alignment must be a power of two
	template<typename T, ENABLE_IF_INTEGRAL_TYPE(T)>
	static T AlignUp(T value, T alignment) noexcept { return (value + alignment - 1) & ~(alignment - 1); }

	// Byte size helpers
	static constexpr u64 KB(u64 count) noexcept { return count * 1024; }
	static constexpr u64 MB(u64 count) noexcept { return KB(count) * 1024; }
	static constexpr u64 GB(u64 count) noexcept { return MB(count) * 1024; }
}