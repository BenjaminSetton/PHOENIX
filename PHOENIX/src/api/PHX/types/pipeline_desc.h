#pragma once

#include "integral_types.h"

namespace PHX
{
	enum class PIPELINE_TYPE
	{
		GRAPHICS = 0,
		COMPUTE,

		MAX
	};

	enum class PRIMITIVE_TOPOLOGY
	{
		POINT_LIST = 0,
		LINE_LIST,
		LINE_STRIP,
		TRIANGLE_LIST,
		TRIANGLE_STRIP,
		TRIANGLE_FAN,

		MAX
	};

	enum class POLYGON_MODE
	{
		FILL = 0,
		LINE,
		POINT,

		MAX
	};

	enum class CULL_MODE
	{
		NONE = 0,
		FRONT,
		BACK,
		FRONT_AND_BACK,

		MAX
	};

	enum class FRONT_FACE_WINDING
	{
		COUNTER_CLOCKWISE = 0,
		CLOCKWISE,

		MAX
	};

	enum class COMPARE_OP
	{
		NEVER = 0,
		LESS,
		EQUAL,
		LESS_OR_EQUAL,
		GREATER,
		NOT_EQUAL,
		GREATER_OR_EQUAL,
		ALWAYS,

		MAX
	};

	enum class STENCIL_OP
	{
		KEEP = 0,
		ZERO,
		REPLACE,
		INCREMENT_AND_CLAMP,
		DECREMENT_AND_CLAMP,
		INVERT,
		INCREMENT_AND_WRAP,
		DECREMENT_AND_WRAP,

		MAX
	};

	struct StencilOpState
	{
		STENCIL_OP failOp;
		STENCIL_OP passOp;
		STENCIL_OP depthFailOp;
		COMPARE_OP compareOp;
		u32 compareMask;
		u32 writeMask;
		u32 reference;
	};
}