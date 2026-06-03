#pragma once

namespace PHX
{
	enum class HANDLE_TYPE
	{
		BUFFER = 0,
		TEXTURE,
		UNIFORM,
		DEVICE_CONTEXT,
		RENDER_PASS,
		RENDER_GRAPH,

		COUNT,
		INVALID
	};
}