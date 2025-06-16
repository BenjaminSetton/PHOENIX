#pragma once

#include "PHX/types/buffer_desc.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	struct BufferCreateInfo
	{
		u64 sizeBytes;
		BUFFER_USAGE bufferUsage;
	};

	class IBuffer
	{
	public:

		virtual ~IBuffer() { }

		virtual STATUS_CODE CopyData(const void* data, u64 size) = 0;
	};
}