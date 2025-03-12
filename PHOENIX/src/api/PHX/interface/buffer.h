#pragma once

#include "PHX/types/buffer_desc.h"
#include "PHX/types/integral_types.h"

namespace PHX
{
	struct BufferCreateInfo
	{
		u64 size;
		BUFFER_USAGE bufferUsage;
	};

	class IBuffer
	{
	public:

		virtual ~IBuffer() { }
	};
}