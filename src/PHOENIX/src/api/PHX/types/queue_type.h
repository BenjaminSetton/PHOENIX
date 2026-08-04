#pragma once

namespace PHX
{
	enum class QUEUE_TYPE
	{
		GRAPHICS = 0,
		PRESENT,
		TRANSFER,
		COMPUTE,

		COUNT
	};
}