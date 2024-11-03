#pragma once

#include <cstdint>

namespace PHX
{
	// unsigned int
	typedef uint8_t u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;

	static constexpr u8  U8_MIN  = 0;
	static constexpr u16 U16_MIN = 0;
	static constexpr u32 U32_MIN = 0;
	static constexpr u64 U64_MIN = 0;

	static constexpr u8  U8_MAX  = UINT8_MAX;
	static constexpr u16 U16_MAX = UINT16_MAX;
	static constexpr u32 U32_MAX = UINT32_MAX;
	static constexpr u64 U64_MAX = UINT64_MAX;


	// signed int
	typedef int8_t i8;
	typedef int16_t i16;
	typedef int32_t i32;
	typedef int64_t i64;

	static constexpr i8  I8_MIN  = INT8_MIN;
	static constexpr i16 I16_MIN = INT16_MIN;
	static constexpr i32 I32_MIN = INT32_MIN;
	static constexpr i64 I64_MIN = INT64_MIN;

	static constexpr i8  I8_MAX  = INT8_MAX;
	static constexpr i16 I16_MAX = INT16_MAX;
	static constexpr i32 I32_MAX = INT32_MAX;
	static constexpr i64 I64_MAX = INT64_MAX;
}