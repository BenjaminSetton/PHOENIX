#pragma once

#include "integral_types.h"
#include "texture_desc.h"

namespace PHX
{
	enum class VERTEX_INPUT_RATE
	{
		PER_VERTEX = 0,
		PER_INSTANCE
	};

	struct InputAttribute
	{
		u32 location;
		u32 binding;
		BASE_FORMAT format;
	};
}