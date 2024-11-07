#pragma once

#include <assert.h>

namespace PHX
{
// Run-time asserts
#define ASSERT(x) assert(x)
#define ASSERT_MSG(x, msg) ASSERT(x && msg)													// Msg parameter _must_ be a const char*
#define ASSERT_PTR(x, msg) ASSERT_MSG(x != nullptr, msg)
#define ASSERT_ALWAYS(msg) ASSERT_MSG(false, msg)

// Compile-time asserts
#define STATIC_ASSERT(x) static_assert(x)
#define STATIC_ASSERT_MSG(x, msg) STATIC_ASSERT(x && msg)
#define ASSERT_SAME_SIZE(x, y) STATIC_ASSERT(sizeof(x) == sizeof(y))

// Utility
#define UNUSED(x) (void)x
#define TODO() ASSERT_MSG(false, "TODO - Implement");										// Utility assert for when incomplete code is run
#define SAFE_DEL(x) do { if(x != nullptr) { delete x; x = nullptr; } } while(false)			// Utility delete macro for safely deleting a non-array pointer (e.g. checking if null and also setting it to null)
#define SAFE_DEL_ARR(x) do { if(x != nullptr) { delete[] x; x = nullptr; } } while(false)	// Utility delete macro for safely deleting an array pointer (e.g. checking if null and also setting it to null)
}