#pragma once

#include "PHX/types/buffer_desc.h"
#include "PHX/types/integral_types.h"

#include "PHX/interface/handle.h"

namespace PHX
{
	struct BufferCreateInfo
	{
		const char* pName		 = "";
		u64 sizeBytes			 = 0;
		BufferUsageFlags bufferUsage = BUFFER_USAGE_FLAG_UNIFORM_BUFFER; // No clear default
	};

	struct PHX_API BufferHandle : public Handle
	{
		DECLARE_PHX_HANDLE(BufferHandle);

		const char* GetName() const;
		BufferUsageFlags GetUsage() const;
		u64 GetSize() const;
	};
}