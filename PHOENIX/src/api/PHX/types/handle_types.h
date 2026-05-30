#pragma once

namespace PHX
{
	enum class HANDLE_TYPE
	{
		BUFFER = 0,
		TEXTURE,
		UNIFORM,
		DEVICE_CONTEXT,

		COUNT,
		INVALID
	};
}