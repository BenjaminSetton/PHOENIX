#pragma once

#include "BSL/integral_types.h"
#include "BSL/vec_types.h"

namespace PHX
{
	struct ClearColor
	{
		BSL::Vec4f color = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	struct ClearDepthStencil
	{
		float depthClear = 1.0f;
		u32 stencilClear = 0;
	};

	struct ClearValues
	{
		ClearColor color;
		ClearDepthStencil depthStencil;
		bool useClearColor = true; // Determines whether to use the values from clear color or clear depth/stencil
	};
}