#pragma once

#include "integral_types.h"
#include "vec_types.h"

namespace PHX
{
	struct ClearColor
	{
		Vec4f color = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	struct ClearDepthStencil
	{
		float depthClear = 1.0f;
		u32 stencilClear = 0;
	};

	struct ClearValues
	{
		union
		{
			ClearColor color;
			ClearDepthStencil depthStencil;
		};
		bool isClearColor = true; // Determines whether the value with the correct data in the union is the clear color or not
	};
}