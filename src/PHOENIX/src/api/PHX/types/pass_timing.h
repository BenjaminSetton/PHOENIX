#pragma once

#include "BSL/integral_types.h"

namespace PHX
{
	static constexpr u32 MAX_PASS_NAME_LEN = 64;

	// Holds the duration of a render pass in milliseconds, along with it's name
	struct PassTiming
	{
		char  passName[MAX_PASS_NAME_LEN] = {};
		float timeInMs                    = 0.0f;
	};
}
