#pragma once

#include <cstdint> // (u)int8_t, (u)int16_t, (u)int32_t, (u)int64_t 
#include <limits>  // std::numeric_limits

namespace PHX
{
	// unsigned int
	typedef uint8_t  u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;

	static constexpr u8  U8_MIN  = 0;
	static constexpr u16 U16_MIN = 0;
	static constexpr u32 U32_MIN = 0;
	static constexpr u64 U64_MIN = 0;

	static constexpr u8  U8_MAX  = std::numeric_limits<u8 >::max();
	static constexpr u16 U16_MAX = std::numeric_limits<u16>::max();
	static constexpr u32 U32_MAX = std::numeric_limits<u32>::max();
	static constexpr u64 U64_MAX = std::numeric_limits<u64>::max();


	// signed int
	typedef int8_t  i8;
	typedef int16_t i16;
	typedef int32_t i32;
	typedef int64_t i64;

	static constexpr i8  I8_MIN  = std::numeric_limits<i8 >::min();
	static constexpr i16 I16_MIN = std::numeric_limits<i16>::min();
	static constexpr i32 I32_MIN = std::numeric_limits<i32>::min();
	static constexpr i64 I64_MIN = std::numeric_limits<i64>::min();

	static constexpr i8  I8_MAX  = std::numeric_limits<i8 >::max();
	static constexpr i16 I16_MAX = std::numeric_limits<i16>::max();
	static constexpr i32 I32_MAX = std::numeric_limits<i32>::max();
	static constexpr i64 I64_MAX = std::numeric_limits<i64>::max();
}