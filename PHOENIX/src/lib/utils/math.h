#pragma once

#include <type_traits> // std::is_integral_v

namespace PHX
{
#define ENABLE_IF_INTEGRAL_TYPE(T) class = typename std::enable_if<std::is_integral_v<T>>::type

	template<typename T, ENABLE_IF_INTEGRAL_TYPE(T)>
	static T Min(const T left, const T right) noexcept { return (left < right ? left : right); }

	template<typename T, ENABLE_IF_INTEGRAL_TYPE(T)>
	static T Max(const T left, const T right) noexcept { return (left > right ? left : right); }

	template<typename T, ENABLE_IF_INTEGRAL_TYPE(T)>
	static T Clamp(T val, T min, T max) noexcept { return Max(Min(val, min), max); }
}